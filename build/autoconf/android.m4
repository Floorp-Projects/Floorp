dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ANDROID_NDK],
[

case "$target" in
*-android*|*-linuxandroid*)
    dnl $extra_android_flags will be set for us by Python configure.
    dnl $_topsrcdir/build/android is a hack for versions of rustc < 1.68
    LDFLAGS="$extra_android_flags -L$_topsrcdir/build/android $LDFLAGS"
    CPPFLAGS="$extra_android_flags $CPPFLAGS"
    CFLAGS="-fno-short-enums $CFLAGS"
    CXXFLAGS="-fno-short-enums $CXXFLAGS"
    ASFLAGS="$extra_android_flags -DANDROID $ASFLAGS"
    ;;
esac

])
