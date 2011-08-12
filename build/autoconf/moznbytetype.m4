dnl ***** BEGIN LICENSE BLOCK *****
dnl Version: MPL 1.1/GPL 2.0/LGPL 2.1
dnl
dnl The contents of this file are subject to the Mozilla Public License Version
dnl 1.1 (the "License"); you may not use this file except in compliance with
dnl the License. You may obtain a copy of the License at
dnl http://www.mozilla.org/MPL/
dnl
dnl Software distributed under the License is distributed on an "AS IS" basis,
dnl WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
dnl for the specific language governing rights and limitations under the
dnl License.
dnl
dnl The Original Code is mozilla.org code.
dnl
dnl The Initial Developer of the Original Code is
dnl   The Mozilla Foundation
dnl Portions created by the Initial Developer are Copyright (C) 2008
dnl the Initial Developer. All Rights Reserved.
dnl
dnl Contributor(s):
dnl   Jim Blandy
dnl
dnl Alternatively, the contents of this file may be used under the terms of
dnl either of the GNU General Public License Version 2 or later (the "GPL"),
dnl or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
dnl in which case the provisions of the GPL or the LGPL are applicable instead
dnl of those above. If you wish to allow use of your version of this file only
dnl under the terms of either the GPL or the LGPL, and not to allow others to
dnl use your version of this file under the terms of the MPL, indicate your
dnl decision by deleting the provisions above and replace them with the notice
dnl and other provisions required by the GPL or the LGPL. If you do not delete
dnl the provisions above, a recipient may use your version of this file under
dnl the terms of any one of the MPL, the GPL or the LGPL.
dnl
dnl ***** END LICENSE BLOCK *****

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
