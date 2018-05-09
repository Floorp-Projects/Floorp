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

    dnl Add --allow-shlib-undefined, because libGLESv2 links to an
    dnl undefined symbol (present on the hardware, just not in the
    dnl NDK.)
    LDFLAGS="-L$android_platform/usr/lib -Wl,-rpath-link=$android_platform/usr/lib --sysroot=$android_platform -Wl,--allow-shlib-undefined $LDFLAGS"
    ANDROID_PLATFORM="${android_platform}"

    AC_DEFINE(ANDROID)
    AC_SUBST(ANDROID_PLATFORM)

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
    mips32) # When target_cpu is mipsel, CPU_ARCH is mips32
        ANDROID_CPU_ARCH=mips
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
dnl Arg 1: compile SDK version, like 23.
dnl Arg 2: target SDK version, like 23.
dnl Arg 3: list of build-tools versions, like "23.0.3 23.0.1".
AC_DEFUN([MOZ_ANDROID_SDK],
[

MOZ_ARG_WITH_STRING(android-sdk,
[  --with-android-sdk=DIR
                          location where the Android SDK can be found (like ~/.mozbuild/android-sdk-linux)],
    android_sdk_root=$withval)

android_sdk_root=${withval%/platforms/android-*}

case "$target" in
*-android*|*-linuxandroid*)
    if test -z "$android_sdk_root" ; then
        AC_MSG_ERROR([You must specify --with-android-sdk=/path/to/sdk when targeting Android.])
    fi

    # We were given an old-style
    # --with-android-sdk=/path/to/sdk/platforms/android-*.  We could warn, but
    # we'll get compliance by forcing the issue.
    if test -e "$withval"/source.properties ; then
        AC_MSG_ERROR([Including platforms/android-* in --with-android-sdk arguments is deprecated.  Use --with-android-sdk=$android_sdk_root.])
    fi

    android_compile_sdk=$1
    AC_MSG_CHECKING([for Android SDK platform version $android_compile_sdk])
    android_sdk=$android_sdk_root/platforms/android-$android_compile_sdk
    if ! test -e "$android_sdk/source.properties" ; then
        AC_MSG_ERROR([You must download Android SDK platform version $android_compile_sdk.  Try |mach bootstrap|.  (Looked for $android_sdk)])
    fi
    AC_MSG_RESULT([$android_sdk])

    android_target_sdk=$2
    if test $android_compile_sdk -lt $android_target_sdk ; then
        AC_MSG_ERROR([Android compileSdkVersion ($android_compile_sdk) should not be smaller than targetSdkVersion ($android_target_sdk).])
    fi

    AC_MSG_CHECKING([for Android build-tools])
    android_build_tools_base="$android_sdk_root"/build-tools
    android_build_tools_version=""
    for version in $3; do
        android_build_tools="$android_build_tools_base"/$version
        if test -d "$android_build_tools" -a -f "$android_build_tools/aapt"; then
            android_build_tools_version=$version
            AC_MSG_RESULT([$android_build_tools])
            break
        fi
    done
    if test "$android_build_tools_version" = ""; then
        version=$(echo $3 | cut -d" " -f1)
        AC_MSG_ERROR([You must install the Android build-tools version $version.  Try |mach bootstrap|.  (Looked for "$android_build_tools_base"/$version)])
    fi

    MOZ_PATH_PROG(ZIPALIGN, zipalign, :, [$android_build_tools])
    if test -z "$ZIPALIGN" -o "$ZIPALIGN" = ":"; then
      AC_MSG_ERROR([The program zipalign was not found.  Try |mach bootstrap|.])
    fi

    android_platform_tools="$android_sdk_root"/platform-tools
    AC_MSG_CHECKING([for Android platform-tools])
    if test -d "$android_platform_tools" -a -f "$android_platform_tools/adb"; then
        AC_MSG_RESULT([$android_platform_tools])
    else
        AC_MSG_ERROR([You must install the Android platform-tools.  Try |mach bootstrap|.  (Looked for $android_platform_tools)])
    fi

    MOZ_PATH_PROG(ADB, adb, :, [$android_platform_tools])
    if test -z "$ADB" -o "$ADB" = ":"; then
      AC_MSG_ERROR([The program adb was not found.  Try |mach bootstrap|.])
    fi

    android_tools="$android_sdk_root"/tools
    AC_MSG_CHECKING([for Android tools])
    if test -d "$android_tools" -a -f "$android_tools/emulator"; then
        AC_MSG_RESULT([$android_tools])
    else
        AC_MSG_ERROR([You must install the Android tools.  Try |mach bootstrap|.  (Looked for $android_tools)])
    fi

    dnl Android Tools 26 changes emulator path.
    dnl Although android_sdk_root/tools still has emulator command,
    dnl it doesn't work correctly
    MOZ_PATH_PROG(EMULATOR, emulator, :, [$android_sdk_root/emulator:$android_tools])
    if test -z "$EMULATOR" -o "$EMULATOR" = ":"; then
        AC_MSG_ERROR([The program emulator was not found.  Try |mach bootstrap|.])
    fi

    ANDROID_COMPILE_SDK_VERSION="${android_compile_sdk}"
    ANDROID_TARGET_SDK="${android_target_sdk}"
    ANDROID_SDK="${android_sdk}"
    ANDROID_SDK_ROOT="${android_sdk_root}"
    ANDROID_TOOLS="${android_tools}"
    ANDROID_BUILD_TOOLS_VERSION="$android_build_tools_version"
    AC_SUBST(ANDROID_COMPILE_SDK_VERSION)
    AC_SUBST(ANDROID_TARGET_SDK)
    AC_SUBST(ANDROID_SDK_ROOT)
    AC_SUBST(ANDROID_SDK)
    AC_SUBST(ANDROID_TOOLS)
    AC_SUBST(ANDROID_BUILD_TOOLS_VERSION)
    ;;
esac

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
