dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Check for the existence of various allocation headers/functions
AC_DEFUN([MOZ_CHECK_ALLOCATOR],[

MALLOC_HEADERS="malloc.h malloc_np.h malloc/malloc.h sys/malloc.h"
MALLOC_H=

for file in $MALLOC_HEADERS; do
  MOZ_CHECK_HEADER($file, [MALLOC_H=$file])
  if test "$MALLOC_H" != ""; then
    AC_DEFINE_UNQUOTED(MALLOC_H, <$MALLOC_H>)
    break
  fi
done

MALLOC_USABLE_SIZE_CONST_PTR=const
if test -n "$HAVE_MALLOC_H"; then
  AC_CACHE_CHECK([whether malloc_usable_size definition can use const argument],
                 moz_cv_malloc_usable_size_constness,
                 [AC_TRY_COMPILE([#include <malloc.h>
                                  #include <stddef.h>
                                  size_t malloc_usable_size(const void *ptr);],
                                 [return malloc_usable_size(0);],
                                 [moz_cv_malloc_usable_size_constness=yes],
                                 [moz_cv_malloc_usable_size_constness=no])])
  if test "$moz_cv_malloc_usable_size_constness" = no ; then
    MALLOC_USABLE_SIZE_CONST_PTR=
  fi
fi
AC_DEFINE_UNQUOTED([MALLOC_USABLE_SIZE_CONST_PTR],[$MALLOC_USABLE_SIZE_CONST_PTR])


dnl In newer bionic headers, valloc is built but not defined,
dnl so we check more carefully here.
AC_MSG_CHECKING([for valloc in malloc.h])
AC_EGREP_HEADER(valloc, malloc.h,
                AC_DEFINE(HAVE_VALLOC)
                AC_MSG_RESULT([yes]),
                AC_MSG_RESULT([no]))

AC_MSG_CHECKING([for valloc in unistd.h])
AC_EGREP_HEADER(valloc, unistd.h,
                AC_DEFINE(HAVE_VALLOC)
                AC_MSG_RESULT([yes]),
                AC_MSG_RESULT([no]))

AC_MSG_CHECKING([for _aligned_malloc in malloc.h])
AC_EGREP_HEADER(_aligned_malloc, malloc.h,
                AC_DEFINE(HAVE_ALIGNED_MALLOC)
                AC_MSG_RESULT([yes]),
                AC_MSG_RESULT([no]))


])
