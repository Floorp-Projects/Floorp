dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN(MOZ_CHECK_COMMON_HEADERS,
	MOZ_CHECK_HEADERS(sys/byteorder.h getopt.h unistd.h nl_types.h \
        malloc.h cpuid.h)
)
