/* fat-setup.h

   Copyright (C) 2015 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

/* Fat library initialization works as follows. The main function is
   fat_init. It tries to do initialization only once, but since it is
   idempotent and pointer updates are atomic on x86_64, there's no
   harm if it is in some cases called multiple times from several
   threads.

   The fat_init function checks the cpuid flags, and sets function
   pointers, e.g, _nettle_aes_encrypt_vec, to point to the appropriate
   implementation.

   To get everything hooked in, we use a belt-and-suspenders approach.

   We try to register fat_init as a constructor function to be called
   at load time. If this is unavailable or non-working, we instead
   arrange fat_init to be called lazily.

   For the actual indirection, there are two cases. 

   If ifunc support is available, function pointers are statically
   initialized to NULL, and we register resolver functions, e.g.,
   _nettle_aes_encrypt_resolve, which call fat_init, and then return
   the function pointer, e.g., the value of _nettle_aes_encrypt_vec.

   If ifunc is not available, we have to define a wrapper function to
   jump via the function pointer. (FIXME: For internal calls, we could
   do this as a macro). We statically initialize each function pointer
   to point to a special initialization function, e.g.,
   _nettle_aes_encrypt_init, which calls fat_init, and then invokes
   the right function. This way, all pointers are setup correctly at
   the first call to any fat function.
*/

#if HAVE_GCC_ATTRIBUTE
# define CONSTRUCTOR __attribute__ ((constructor))
#else
# define CONSTRUCTOR
# if defined (__sun)
#  pragma init(fat_init)
# endif
#endif

#define ENV_VERBOSE "NETTLE_FAT_VERBOSE"

/* DECLARE_FAT_FUNC(name, ftype)
 *
 *   name is the public function, e.g., _nettle_aes_encrypt.
 *   ftype is its type, e.g., aes_crypt_internal_func.
 *
 * DECLARE_FAT_VAR(name, type, var)
 *
 *   name is name without _nettle prefix.
 *   type is its type.
 *   var is the variant, used as a suffix on the symbol name.
 *
 * DEFINE_FAT_FUNC(name, rtype, prototype, args)
 *
 *   name is the public function.
 *   rtype its return type.
 *   prototype is the list of formal arguments, with types.
 *   args contain the argument list without any types.
 */

#if HAVE_LINK_IFUNC
#define IFUNC(resolve) __attribute__ ((ifunc (resolve)))
#define DECLARE_FAT_FUNC(name, ftype)	\
  ftype name IFUNC(#name"_resolve");	\
  static ftype *name##_vec = NULL;

#define DEFINE_FAT_FUNC(name, rtype, prototype, args)		  \
  static void_func * name##_resolve(void)			  \
  {								  \
    if (getenv (ENV_VERBOSE))					  \
      fprintf (stderr, "libnettle: "#name"_resolve\n");		  \
    fat_init();							  \
    return (void_func *) name##_vec;				  \
  }

#else /* !HAVE_LINK_IFUNC */
#define DECLARE_FAT_FUNC(name, ftype)		\
  ftype name;					\
  static ftype name##_init;			\
  static ftype *name##_vec = name##_init;				

#define DEFINE_FAT_FUNC(name, rtype, prototype, args)		\
  rtype name prototype						\
  {								\
    return name##_vec args;					\
  }								\
  static rtype name##_init prototype {				\
    if (getenv (ENV_VERBOSE))					\
      fprintf (stderr, "libnettle: "#name"_init\n");		\
    fat_init();							\
    assert (name##_vec != name##_init);				\
    return name##_vec args;					\
  }
#endif /* !HAVE_LINK_IFUNC */

#define DECLARE_FAT_FUNC_VAR(name, type, var)	\
       type _nettle_##name##_##var;

typedef void void_func (void);

typedef void aes_crypt_internal_func (unsigned rounds, const uint32_t *keys,
				      const struct aes_table *T,
				      size_t length, uint8_t *dst,
				      const uint8_t *src);

typedef void *(memxor_func)(void *dst, const void *src, size_t n);
