dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ANDROID_NDK],
[

case "$target" in
*-android*|*-linuxandroid*)
    dnl $android_platform will be set for us by Python configure.
    directory_include_args="-isystem $android_system -isystem $android_sysroot/usr/include"

    # clang will do any number of interesting things with host tools unless we tell
    # it to use the NDK tools.
    extra_opts="-gcc-toolchain $(dirname $(dirname $TOOLCHAIN_PREFIX))"
    CPPFLAGS="$extra_opts -D__ANDROID_API__=$android_version $CPPFLAGS"
    ASFLAGS="$extra_opts $ASFLAGS"
    LDFLAGS="$extra_opts $LDFLAGS"

    CPPFLAGS="$directory_include_args $CPPFLAGS"
    CFLAGS="-fno-short-enums -fno-exceptions $CFLAGS"
    CXXFLAGS="-fno-short-enums -fno-exceptions $CXXFLAGS $stlport_cppflags"
    ASFLAGS="$directory_include_args -DANDROID $ASFLAGS"

    LDFLAGS="-L$android_platform/usr/lib -Wl,-rpath-link=$android_platform/usr/lib --sysroot=$android_platform $LDFLAGS"
    ;;
esac

])

AC_DEFUN([MOZ_ANDROID_CPU_ARCH],
[

if test "$OS_TARGET" = "Android"; then
    case "${CPU_ARCH}" in
    arm)
        ANDROID_CPU_ARCH=armeabi-v7a
        ;;
    x86)
        ANDROID_CPU_ARCH=x86
        ;;
    x86_64)
        ANDROID_CPU_ARCH=x86_64
        ;;
    aarch64)
        ANDROID_CPU_ARCH=arm64-v8a
        ;;
    esac

    AC_SUBST(ANDROID_CPU_ARCH)
fi
])

AC_DEFUN([MOZ_ANDROID_STLPORT],
[

if test "$OS_TARGET" = "Android"; then
    if test -z "$STLPORT_LIBS"; then
        # android-ndk-r8b and later
        cxx_libs="$android_ndk/sources/cxx-stl/llvm-libc++/libs/$ANDROID_CPU_ARCH"
        # NDK r12 removed the arm/thumb library split and just made
        # everything thumb by default.  Attempt to compensate.
        if test "$MOZ_THUMB2" = 1 -a -d "$cxx_libs/thumb"; then
            cxx_libs="$cxx_libs/thumb"
        fi

        if ! test -e "$cxx_libs/libc++_static.a"; then
            AC_MSG_ERROR([Couldn't find path to llvm-libc++ in the android ndk])
        fi

        STLPORT_LIBS="-L$cxx_libs -lc++_static"
        # NDK r12 split the libc++ runtime libraries into pieces.
        for lib in c++abi unwind android_support; do
            if test -e "$cxx_libs/lib${lib}.a"; then
                 STLPORT_LIBS="$STLPORT_LIBS -l${lib}"
            fi
        done
    fi
fi
AC_SUBST_LIST([STLPORT_LIBS])

])


dnl Configure an Android SDK.
AC_DEFUN([MOZ_ANDROID_SDK],
[

MOZ_ARG_WITH_STRING(android-min-sdk,
[  --with-android-min-sdk=[VER]     Impose a minimum Firefox for Android SDK version],
[ MOZ_ANDROID_MIN_SDK_VERSION=$withval ])

MOZ_ARG_WITH_STRING(android-max-sdk,
[  --with-android-max-sdk=[VER]     Impose a maximum Firefox for Android SDK version],
[ MOZ_ANDROID_MAX_SDK_VERSION=$withval ])

if test -n "$MOZ_ANDROID_MIN_SDK_VERSION"; then
    if test -n "$MOZ_ANDROID_MAX_SDK_VERSION"; then
        if test $MOZ_ANDROID_MAX_SDK_VERSION -lt $MOZ_ANDROID_MIN_SDK_VERSION ; then
            AC_MSG_ERROR([--with-android-max-sdk must be at least the value of --with-android-min-sdk.])
        fi
    fi

    if test $MOZ_ANDROID_MIN_SDK_VERSION -gt $ANDROID_TARGET_SDK ; then
        AC_MSG_ERROR([--with-android-min-sdk is expected to be less than $ANDROID_TARGET_SDK])
    fi

    AC_SUBST(MOZ_ANDROID_MIN_SDK_VERSION)
fi

AC_SUBST(MOZ_ANDROID_MAX_SDK_VERSION)

])
