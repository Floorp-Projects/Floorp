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

MOZ_ARG_WITH_STRING(android-gnu-compiler-version,
[  --with-android-gnu-compiler-version=VER
                          gnu compiler version to use],
    android_gnu_compiler_version=$withval)

MOZ_ARG_ENABLE_BOOL(android-libstdcxx,
[  --enable-android-libstdcxx
                          use GNU libstdc++ instead of STLPort],
    MOZ_ANDROID_LIBSTDCXX=1,
    MOZ_ANDROID_LIBSTDCXX= )

define([MIN_ANDROID_VERSION], [9])
android_version=MIN_ANDROID_VERSION

MOZ_ARG_WITH_STRING(android-version,
[  --with-android-version=VER
                          android platform version, default] MIN_ANDROID_VERSION,
    android_version=$withval)

if test $android_version -lt MIN_ANDROID_VERSION ; then
    AC_MSG_ERROR([--with-android-version must be at least MIN_ANDROID_VERSION.])
fi

MOZ_ARG_WITH_STRING(android-platform,
[  --with-android-platform=DIR
                           location of platform dir],
    android_platform=$withval)

case "$target" in
arm-linux*-android*|*-linuxandroid*)
    android_tool_prefix="arm-linux-androideabi"
    ;;
i?86-*android*)
    android_tool_prefix="i686-linux-android"
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

        for version in $android_gnu_compiler_version 4.7 4.6 4.4.3 ; do
            case "$target_cpu" in
            arm)
                target_name=arm-linux-androideabi-$version
                ;;
            i?86)
                target_name=x86-$version
                ;;
            mipsel)
                target_name=mipsel-linux-android-$version
                ;;
            *)
                AC_MSG_ERROR([target cpu is not supported])
                ;;
            esac
            case "$host_cpu" in
            i*86)
                android_toolchain="$android_ndk"/toolchains/$target_name/prebuilt/$kernel_name-x86
                ;;
            x86_64)
                android_toolchain="$android_ndk"/toolchains/$target_name/prebuilt/$kernel_name-x86_64
                if ! test -d "$android_toolchain" ; then
                    android_toolchain="$android_ndk"/toolchains/$target_name/prebuilt/$kernel_name-x86
                fi
                ;;
            *)
                AC_MSG_ERROR([No known toolchain for your host cpu])
                ;;
            esac
            if test -d "$android_toolchain" ; then
                android_gnu_compiler_version=$version
                break
            elif test -n "$android_gnu_compiler_version" ; then
                AC_MSG_ERROR([not found. Your --with-android-gnu-compiler-version may be wrong.])
            fi
        done

        if test -z "$android_gnu_compiler_version" ; then
            AC_MSG_ERROR([not found. You have to specify --with-android-toolchain=/path/to/ndk/toolchain.])
        else
            AC_MSG_RESULT([$android_toolchain])
        fi
        NSPR_CONFIGURE_ARGS="$NSPR_CONFIGURE_ARGS --with-android-toolchain=$android_toolchain"
    fi

    NSPR_CONFIGURE_ARGS="$NSPR_CONFIGURE_ARGS --with-android-version=$android_version"

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
        NSPR_CONFIGURE_ARGS="$NSPR_CONFIGURE_ARGS --with-android-platform=$android_platform"
    fi

    dnl Old NDK support. If minimum requirement is changed to NDK r8b,
    dnl please remove this.
    case "$target_cpu" in
    i?86)
        if ! test -e "$android_toolchain"/bin/"$android_tool_prefix"-gcc; then
            dnl Old NDK toolchain name
            android_tool_prefix="i686-android-linux"
        fi
        ;;
    esac

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

    AC_DEFINE(ANDROID)
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

    AC_SUBST(ANDROID_CPU_ARCH)

    if test -z "$STLPORT_CPPFLAGS$STLPORT_LDFLAGS$STLPORT_LIBS"; then
        if test -n "$MOZ_ANDROID_LIBSTDCXX" ; then
            if test -e "$android_ndk/sources/cxx-stl/gnu-libstdc++/$android_gnu_compiler_version/libs/$ANDROID_CPU_ARCH/libgnustl_static.a"; then
                # android-ndk-r8b
                STLPORT_LDFLAGS="-L$android_ndk/sources/cxx-stl/gnu-libstdc++/$android_gnu_compiler_version/libs/$ANDROID_CPU_ARCH/"
                STLPORT_CPPFLAGS="-I$android_ndk/sources/cxx-stl/gnu-libstdc++/$android_gnu_compiler_version/include -I$android_ndk/sources/cxx-stl/gnu-libstdc++/$android_gnu_compiler_version/libs/$ANDROID_CPU_ARCH/include"
                STLPORT_LIBS="-lgnustl_static"
            elif test -e "$android_ndk/sources/cxx-stl/gnu-libstdc++/libs/$ANDROID_CPU_ARCH/libgnustl_static.a"; then
                # android-ndk-r7, android-ndk-r7b, android-ndk-r8
                STLPORT_LDFLAGS="-L$android_ndk/sources/cxx-stl/gnu-libstdc++/libs/$ANDROID_CPU_ARCH/"
                STLPORT_CPPFLAGS="-I$android_ndk/sources/cxx-stl/gnu-libstdc++/include -I$android_ndk/sources/cxx-stl/gnu-libstdc++/libs/$ANDROID_CPU_ARCH/include"
                STLPORT_LIBS="-lgnustl_static"
            elif test -e "$android_ndk/sources/cxx-stl/gnu-libstdc++/libs/$ANDROID_CPU_ARCH/libstdc++.a"; then
                # android-ndk-r5c, android-ndk-r6, android-ndk-r6b
                STLPORT_CPPFLAGS="-I$android_ndk/sources/cxx-stl/gnu-libstdc++/include -I$android_ndk/sources/cxx-stl/gnu-libstdc++/libs/$ANDROID_CPU_ARCH/include"
                STLPORT_LDFLAGS="-L$android_ndk/sources/cxx-stl/gnu-libstdc++/libs/$ANDROID_CPU_ARCH/"
                STLPORT_LIBS="-lstdc++"
            else
                AC_MSG_ERROR([Couldn't find path to gnu-libstdc++ in the android ndk])
            fi
        elif test -e "$android_ndk/sources/cxx-stl/stlport/src/iostream.cpp" ; then
            if test -e "$android_ndk/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/libstlport_static.a"; then
                STLPORT_LDFLAGS="-L$_objdir/build/stlport -L$android_ndk/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/"
            elif test -e "$android_ndk/tmp/ndk-digit/build/install/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/libstlport_static.a"; then
                STLPORT_LDFLAGS="-L$_objdir/build/stlport -L$android_ndk/tmp/ndk-digit/build/install/sources/cxx-stl/stlport/libs/$ANDROID_CPU_ARCH/"
            else
                AC_MSG_ERROR([Couldn't find path to stlport in the android ndk])
            fi
            STLPORT_SOURCES="$android_ndk/sources/cxx-stl/stlport"
            STLPORT_CPPFLAGS="-I$_objdir/build/stlport -I$android_ndk/sources/cxx-stl/stlport/stlport"
            STLPORT_LIBS="-lstlport_static -static-libstdc++"
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
    # The build tools got moved around to different directories in
    # SDK Tools r22.  Try to locate them.
    android_build_tools=""
    for suffix in 17.0.0 android-4.2.2; do
        tools_directory="$android_sdk/../../build-tools/$suffix"
        if test -d "$tools_directory" ; then
            android_build_tools="$tools_directory"
            break
        fi
    done
    if test -z "$android_build_tools" ; then
        android_build_tools="$android_platform_tools" # SDK Tools < r22
    fi
    ANDROID_SDK="${android_sdk}"
    if test -e "${android_sdk}/../../extras/android/compatibility/v4/android-support-v4.jar" ; then
        ANDROID_COMPAT_LIB="${android_sdk}/../../extras/android/compatibility/v4/android-support-v4.jar"
    else
        ANDROID_COMPAT_LIB="${android_sdk}/../../extras/android/support/v4/android-support-v4.jar";
    fi
    ANDROID_PLATFORM_TOOLS="${android_platform_tools}"
    ANDROID_BUILD_TOOLS="${android_build_tools}"
    AC_SUBST(ANDROID_SDK)
    AC_SUBST(ANDROID_COMPAT_LIB)
    if ! test -e $ANDROID_COMPAT_LIB ; then
        AC_MSG_ERROR([You must download the Android support library when targeting Android.   Run the Android SDK tool and install Android Support Library under Extras.  See https://developer.android.com/tools/extras/support-library.html for more info. (looked for $ANDROID_COMPAT_LIB)])
    fi
    AC_SUBST(ANDROID_PLATFORM_TOOLS)
    AC_SUBST(ANDROID_BUILD_TOOLS)
    ;;
esac

])
