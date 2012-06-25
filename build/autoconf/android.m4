dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ANDROID_NDK],
[

MOZ_ARG_WITH_STRING(android-ndk,
[  --with-android-ndk=DIR
                          location where the Android NDK can be found],
    android_ndk=$withval)

MOZ_ARG_WITH_STRING(android-toolchain,
[  --with-android-toolchain=DIR
                          location of the android toolchain],
    android_toolchain=$withval)


MOZ_ARG_WITH_STRING(android-version,
[  --with-android-version=VER
                          android platform version, default 5],
    android_version=$withval,
    android_version=5)

MOZ_ARG_WITH_STRING(android-platform,
[  --with-android-platform=DIR
                           location of platform dir],
    android_platform=$withval)

case "$target" in
arm-linux*-android*|*-linuxandroid*)
    android_tool_prefix="arm-linux-androideabi"
    ;;
i?86-*android*)
    android_tool_prefix="i686-android-linux"
    ;;
mipsel-*android*)
    android_tool_prefix="mipsel-linux-android"
    ;;
*)
    android_tool_prefix="$target_os"
    ;;
esac

case "$target" in
*-android*|*-linuxandroid*)
    if test -z "$android_ndk" ; then
        AC_MSG_ERROR([You must specify --with-android-ndk=/path/to/ndk when targeting Android.])
    fi

    if test -z "$android_toolchain" ; then
        AC_MSG_CHECKING([for android toolchain directory])

        kernel_name=`uname -s | tr "[[:upper:]]" "[[:lower:]]"`

        case "$target_cpu" in
        arm)
            target_name=arm-linux-androideabi-4.4.3
            ;;
        i?86)
            target_name=x86-4.4.3
            ;;
        mipsel)
            target_name=mipsel-linux-android-4.4.3
            ;;
        esac
        android_toolchain="$android_ndk"/toolchains/$target_name/prebuilt/$kernel_name-x86

        if test -d "$android_toolchain" ; then
            AC_MSG_RESULT([$android_toolchain])
        else
            AC_MSG_ERROR([not found. You have to specify --with-android-toolchain=/path/to/ndk/toolchain.])
        fi
    fi

    if test -z "$android_platform" ; then
        AC_MSG_CHECKING([for android platform directory])

        case "$target_cpu" in
        arm)
            target_name=arm
            ;;
        i?86)
            target_name=x86
            ;;
        mipsel)
            target_name=mips
            ;;
        esac

        android_platform="$android_ndk"/platforms/android-"$android_version"/arch-"$target_name"

        if test -d "$android_platform" ; then
            AC_MSG_RESULT([$android_platform])
        else
            AC_MSG_ERROR([not found. You have to specify --with-android-platform=/path/to/ndk/platform.])
        fi
    fi

    dnl set up compilers
    TOOLCHAIN_PREFIX="$android_toolchain/bin/$android_tool_prefix-"
    AS="$android_toolchain"/bin/"$android_tool_prefix"-as
    CC="$android_toolchain"/bin/"$android_tool_prefix"-gcc
    CXX="$android_toolchain"/bin/"$android_tool_prefix"-g++
    CPP="$android_toolchain"/bin/"$android_tool_prefix"-cpp
    LD="$android_toolchain"/bin/"$android_tool_prefix"-ld
    AR="$android_toolchain"/bin/"$android_tool_prefix"-ar
    RANLIB="$android_toolchain"/bin/"$android_tool_prefix"-ranlib
    STRIP="$android_toolchain"/bin/"$android_tool_prefix"-strip
    OBJCOPY="$android_toolchain"/bin/"$android_tool_prefix"-objcopy

    CPPFLAGS="-isystem $android_platform/usr/include $CPPFLAGS"
    CFLAGS="-mandroid -fno-short-enums -fno-exceptions $CFLAGS"
    CXXFLAGS="-mandroid -fno-short-enums -fno-exceptions -Wno-psabi $CXXFLAGS"
    ASFLAGS="-isystem $android_platform/usr/include -DANDROID $ASFLAGS"

    dnl Add -llog by default, since we use it all over the place.
    dnl Add --allow-shlib-undefined, because libGLESv2 links to an
    dnl undefined symbol (present on the hardware, just not in the
    dnl NDK.)
    LDFLAGS="-mandroid -L$android_platform/usr/lib -Wl,-rpath-link=$android_platform/usr/lib --sysroot=$android_platform -llog -Wl,--allow-shlib-undefined $LDFLAGS"
    dnl prevent cross compile section from using these flags as host flags
    if test -z "$HOST_CPPFLAGS" ; then
        HOST_CPPFLAGS=" "
    fi
    if test -z "$HOST_CFLAGS" ; then
        HOST_CFLAGS=" "
    fi
    if test -z "$HOST_CXXFLAGS" ; then
        HOST_CXXFLAGS=" "
    fi
    if test -z "$HOST_LDFLAGS" ; then
        HOST_LDFLAGS=" "
    fi

    ANDROID_NDK="${android_ndk}"
    ANDROID_TOOLCHAIN="${android_toolchain}"
    ANDROID_PLATFORM="${android_platform}"
    ANDROID_VERSION="${android_version}"

    AC_DEFINE(ANDROID)
    AC_DEFINE_UNQUOTED(ANDROID_VERSION, $android_version)
    AC_SUBST(ANDROID_VERSION)
    CROSS_COMPILE=1
    AC_SUBST(ANDROID_NDK)
    AC_SUBST(ANDROID_TOOLCHAIN)
    AC_SUBST(ANDROID_PLATFORM)

    ;;
esac

])
    
AC_DEFUN([MOZ_ANDROID_STLPORT],
[

if test "$OS_TARGET" = "Android" -a -z "$gonkdir"; then
    case "${CPU_ARCH}-${MOZ_ARCH}" in
    arm-armv7*)
        ANDROID_CPU_ARCH=armeabi-v7a
        ;;
    arm-*)
        ANDROID_CPU_ARCH=armeabi
        ;;
    x86-*)
        ANDROID_CPU_ARCH=x86
        ;;
    mips-*) # When target_cpu is mipsel, CPU_ARCH is mips
        ANDROID_CPU_ARCH=mips
        ;;
    esac

    if test -z "$STLPORT_CPPFLAGS$STLPORT_LDFLAGS$STLPORT_LIBS"; then
        if test -e "$android_ndk/sources/cxx-stl/stlport/src/iostream.cpp" ; then
            if test -e "$android_ndk/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/libstlport_static.a"; then
                STLPORT_LDFLAGS="-L$_objdir/build/stlport -L$android_ndk/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/"
            elif test -e "$android_ndk/tmp/ndk-digit/build/install/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/libstlport_static.a"; then
                STLPORT_LDFLAGS="-L$_objdir/build/stlport -L$android_ndk/tmp/ndk-digit/build/install/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/"
            else
                AC_MSG_ERROR([Couldn't find path to stlport in the android ndk])
            fi
            STLPORT_SOURCES="$android_ndk/sources/cxx-stl/stlport"
            STLPORT_CPPFLAGS="-I$_objdir/build/stlport -I$android_ndk/sources/cxx-stl/stlport/stlport"
            STLPORT_LIBS="-lstlport_static"
        elif test "$target" != "arm-android-eabi"; then
            dnl fail if we're not building with NDKr4
            AC_MSG_ERROR([Couldn't find path to stlport in the android ndk])
        fi
    fi
    CXXFLAGS="$CXXFLAGS $STLPORT_CPPFLAGS"
    LDFLAGS="$LDFLAGS $STLPORT_LDFLAGS"
    LIBS="$LIBS $STLPORT_LIBS"
fi
AC_SUBST([STLPORT_SOURCES])

])

AC_DEFUN([MOZ_ANDROID_SDK],
[

MOZ_ARG_WITH_STRING(android-sdk,
[  --with-android-sdk=DIR
                          location where the Android SDK can be found (base directory, e.g. .../android/platforms/android-6)],
    android_sdk=$withval)

case "$target" in
*-android*|*-linuxandroid*)
    if test -z "$android_sdk" ; then
        AC_MSG_ERROR([You must specify --with-android-sdk=/path/to/sdk when targeting Android.])
    else
        if ! test -e "$android_sdk"/source.properties ; then
            AC_MSG_ERROR([The path in --with-android-sdk isn't valid (source.properties hasn't been found).])
        fi

        # Get the api level from "$android_sdk"/source.properties.
        android_api_level=`$AWK -F = changequote(<<, >>)'<<$>>1 == "AndroidVersion.ApiLevel" {print <<$>>2}'changequote([, ]) "$android_sdk"/source.properties`

        if test -z "$android_api_level" ; then
            AC_MSG_ERROR([Unexpected error: no AndroidVersion.ApiLevel field has been found in source.properties.])
        fi

        if ! test "$android_api_level" -eq "$android_api_level" ; then
            AC_MSG_ERROR([Unexpected error: the found android api value isn't a number! (found $android_api_level)])
        fi

        if test $android_api_level -lt $1 ; then
            AC_MSG_ERROR([The given Android SDK provides API level $android_api_level ($1 or higher required).])
        fi
    fi

    android_platform_tools="$android_sdk"/../../platform-tools
    if test ! -d "$android_platform_tools" ; then
        android_platform_tools="$android_sdk"/tools # SDK Tools < r8
    fi
    ANDROID_SDK="${android_sdk}"
    ANDROID_PLATFORM_TOOLS="${android_platform_tools}"
    AC_SUBST(ANDROID_SDK)
    AC_SUBST(ANDROID_PLATFORM_TOOLS)
    ;;
esac

])
