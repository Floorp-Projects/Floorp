dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_LINUX_PERF_EVENT],
[

MOZ_ARG_WITH_STRING(linux-headers,
[  --with-linux-headers=DIR
                          location where the Linux kernel headers can be found],
    linux_headers=$withval)

LINUX_HEADERS_INCLUDES=

if test "$linux_headers"; then
    LINUX_HEADERS_INCLUDES="-I$linux_headers"
fi

_SAVE_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $LINUX_HEADERS_INCLUDES"

dnl Performance measurement headers.
MOZ_CHECK_HEADER(linux/perf_event.h,
    [AC_CACHE_CHECK(for perf_event_open system call,ac_cv_perf_event_open,
        [AC_TRY_COMPILE([#include <asm/unistd.h>],[return sizeof(__NR_perf_event_open);],
        ac_cv_perf_event_open=yes,
        ac_cv_perf_event_open=no)])])
if test "$ac_cv_perf_event_open" = "yes"; then
    HAVE_LINUX_PERF_EVENT_H=1
else
    HAVE_LINUX_PERF_EVENT_H=
    LINUX_HEADERS_INCLUDES=
fi
AC_SUBST(HAVE_LINUX_PERF_EVENT_H)
AC_SUBST(LINUX_HEADERS_INCLUDES)

CFLAGS="$_SAVE_CFLAGS"

])
