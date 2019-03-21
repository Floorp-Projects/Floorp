dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ARCH_OPTS],
[
if test -n "$_ARM_FLAGS"; then
    CFLAGS="$CFLAGS $_ARM_FLAGS"
    CXXFLAGS="$CXXFLAGS $_ARM_FLAGS"
    ASFLAGS="$ASFLAGS $_ARM_FLAGS"
    if test -n "$_THUMB_FLAGS"; then
        LDFLAGS="$LDFLAGS $_THUMB_FLAGS"
    fi
fi
])
