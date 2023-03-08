dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ANDROID_NDK],
[

case "$target" in
*-android*|*-linuxandroid*)
    dnl $extra_android_flags will be set for us by Python configure.
    LDFLAGS="$extra_android_flags $LDFLAGS"
    CPPFLAGS="$extra_android_flags $CPPFLAGS"
    CFLAGS="-fno-short-enums $CFLAGS"
    CXXFLAGS="-fno-short-enums $CXXFLAGS $stlport_cppflags"
    ASFLAGS="$extra_android_flags -DANDROID $ASFLAGS"
    ;;
esac

])
