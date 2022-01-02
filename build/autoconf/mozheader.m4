dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl MOZ_CHECK_HEADER(HEADER-FILE, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, INCLUDES]]])
AC_DEFUN([MOZ_CHECK_HEADER],
[ dnl Do the transliteration at runtime so arg 1 can be a shell variable.
  ac_safe=`echo "$1" | sed 'y%./+-%__p_%'`
  AC_MSG_CHECKING([for $1])
  AC_CACHE_VAL(ac_cv_header_$ac_safe,
 [ AC_TRY_COMPILE([$4
#include <$1>], ,
                  eval "ac_cv_header_$ac_safe=yes",
                  eval "ac_cv_header_$ac_safe=no") ])
  if eval "test \"`echo '$ac_cv_header_'$ac_safe`\" = yes"; then
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])
  else
    AC_MSG_RESULT(no)
    ifelse([$3], , , [$3])
  fi
])

dnl MOZ_CHECK_HEADERS(HEADER-FILE... [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, INCLUDES]]])
AC_DEFUN([MOZ_CHECK_HEADERS],
[ for ac_hdr in $1
  do
    MOZ_CHECK_HEADER($ac_hdr,
                     [ ac_tr_hdr=HAVE_`echo $ac_hdr | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
                       AC_DEFINE_UNQUOTED($ac_tr_hdr) $2], $3, [$4])
  done
])
