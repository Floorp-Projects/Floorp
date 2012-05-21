dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl MOZ_N_BYTE_TYPE(VARIABLE, SIZE, POSSIBLE-TYPES)
dnl
dnl Check to see which of POSSIBLE-TYPES has a size of SIZE.  If we
dnl find one, define VARIABLE to be the size-BYTE type.  If no type
dnl matches, exit the configure script with an error message.  Types
dnl whose written form contains spaces should appear in POSSIBLE-TYPES
dnl enclosed by shell quotes.
dnl
dnl The cache variable moz_cv_n_byte_type_VARIABLE gets set to the
dnl type, if found.
dnl 
dnl for example:
dnl MOZ_N_BYTE_TYPE([JS_INT32_T], [4], [int long 'long long' short])
dnl
AC_DEFUN([MOZ_N_BYTE_TYPE],
[
dnl The simplest approach would simply be to run a program that says
dnl   printf ("%d\n", sizeof ($type));
dnl But that won't work when cross-compiling; this will.
AC_CACHE_CHECK([for a $2-byte type], moz_cv_n_byte_type_$1, [
  moz_cv_n_byte_type_$1=
  for type in $3; do
    AC_TRY_COMPILE([],
                   [
                     int a[sizeof ($type) == $2 ? 1 : -1];
                     return 0;
                   ],
                   [moz_cv_n_byte_type_$1=$type; break], [])
  done
  if test ! "$moz_cv_n_byte_type_$1"; then
    AC_MSG_ERROR([Couldn't find a $2-byte type])
  fi
])
AC_DEFINE_UNQUOTED($1, [$moz_cv_n_byte_type_$1],
                   [a $2-byte type on the target machine])
])

dnl MOZ_SIZE_OF_TYPE(VARIABLE, TYPE, POSSIBLE-SIZES)
dnl
dnl Check to see which of POSSIBLE-SIZES is the sizeof(TYPE). If we find one,
dnl define VARIABLE SIZE. If no size matches, exit the configure script with
dnl an error message.
dnl
dnl The cache variable moz_cv_size_of_VARIABLE gets set to the size, if
dnl found.
dnl
dnl for example:
dnl MOZ_SIZE_OF_TYPE([JS_BYTES_PER_WORD], [void*], [4 8])
AC_DEFUN([MOZ_SIZE_OF_TYPE],
[
AC_CACHE_CHECK([for the size of $2], moz_cv_size_of_$1, [
  moz_cv_size_of_$1=
  for size in $3; do
    AC_TRY_COMPILE([],
                   [
                     int a[sizeof ($2) == $size ? 1 : -1];
                     return 0;
                   ],
                   [moz_cv_size_of_$1=$size; break], [])
  done
  if test ! "$moz_cv_size_of_$1"; then
    AC_MSG_ERROR([No size found for $2])
  fi
])
AC_DEFINE_UNQUOTED($1, [$moz_cv_size_of_$1])
])

dnl MOZ_ALIGN_OF_TYPE(VARIABLE, TYPE, POSSIBLE-ALIGNS)
dnl
dnl Check to see which of POSSIBLE-ALIGNS is the necessary alignment of TYPE.
dnl If we find one, define VARIABLE ALIGNMENT. If no alignment matches, exit
dnl the configure script with an error message.
dnl
dnl The cache variable moz_cv_align_of_VARIABLE gets set to the size, if
dnl found.
dnl
dnl for example:
dnl MOZ_ALIGN_OF_TYPE(JS_ALIGN_OF_POINTER, void*, 2 4 8 16)
AC_DEFUN([MOZ_ALIGN_OF_TYPE],
[
AC_CACHE_CHECK([for the alignment of $2], moz_cv_align_of_$1, [
  moz_cv_align_of_$1=
  for align in $3; do
    AC_TRY_COMPILE([
                     #include <stddef.h>
                     struct aligner { char c; $2 a; };
                   ],
                   [
                     int a[offsetof(struct aligner, a) == $align ? 1 : -1];
                     return 0;
                   ],
                   [moz_cv_align_of_$1=$align; break], [])
  done
  if test ! "$moz_cv_align_of_$1"; then
    AC_MSG_ERROR([No alignment found for $2])
  fi
])
AC_DEFINE_UNQUOTED($1, [$moz_cv_align_of_$1])
])
