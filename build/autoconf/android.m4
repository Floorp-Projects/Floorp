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

case "$target" in
arm-*linux*-android*|*-linuxandroid*)
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

        for version in $android_gnu_compiler_version 4.9 4.8 4.7; do
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
        AC_MSG_ERROR([not found. Please check your NDK. With the current configuration, it should be in $android_platform])
    fi

    dnl set up compilers
    TOOLCHAIN_PREFIX="$android_toolchain/bin/$android_tool_prefix-"
    AS="$android_toolchain"/bin/"$android_tool_prefix"-as
    if test -z "$CC"; then
        CC="$android_toolchain"/bin/"$android_tool_prefix"-gcc
    fi
    if test -z "$CXX"; then
        CXX="$android_toolchain"/bin/"$android_tool_prefix"-g++
    fi
    if test -z "$CPP"; then
        CPP="$android_toolchain"/bin/"$android_tool_prefix"-cpp
    fi
    LD="$android_toolchain"/bin/"$android_tool_prefix"-ld
    AR="$android_toolchain"/bin/"$android_tool_prefix"-ar
    RANLIB="$android_toolchain"/bin/"$android_tool_prefix"-ranlib
    STRIP="$android_toolchain"/bin/"$android_tool_prefix"-strip
    OBJCOPY="$android_toolchain"/bin/"$android_tool_prefix"-objcopy

    CPPFLAGS="-idirafter $android_platform/usr/include $CPPFLAGS"
    CFLAGS="-mandroid -fno-short-enums -fno-exceptions $CFLAGS"
    CXXFLAGS="-mandroid -fno-short-enums -fno-exceptions -Wno-psabi $CXXFLAGS"
    ASFLAGS="-idirafter $android_platform/usr/include -DANDROID $ASFLAGS"

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

    if test -z "$STLPORT_CPPFLAGS$STLPORT_LIBS"; then
        if test -n "$MOZ_ANDROID_LIBSTDCXX" ; then
            # android-ndk-r8b and later
            ndk_base="$android_ndk/sources/cxx-stl/gnu-libstdc++/$android_gnu_compiler_version"
            ndk_libs="$ndk_base/libs/$ANDROID_CPU_ARCH"
            ndk_include="$ndk_base/include"

            if test -e "$ndk_libs/libgnustl_static.a"; then
                STLPORT_LIBS="-L$ndk_libs -lgnustl_static"
                STLPORT_CPPFLAGS="-I$ndk_include -I$ndk_include/backward -I$ndk_libs/include"
            else
                AC_MSG_ERROR([Couldn't find path to gnu-libstdc++ in the android ndk])
            fi
        else
            STLPORT_CPPFLAGS="-isystem $_topsrcdir/build/stlport/stlport -isystem $_topsrcdir/build/stlport/overrides -isystem $android_ndk/sources/cxx-stl/system/include"
        fi
    fi
    CXXFLAGS="$CXXFLAGS $STLPORT_CPPFLAGS"
fi
AC_SUBST([MOZ_ANDROID_LIBSTDCXX])
AC_SUBST([STLPORT_LIBS])

])


AC_DEFUN([concat],[$1$2$3$4])

dnl Find a component of an AAR.
dnl Arg 1: variable name to expose, like ANDROID_SUPPORT_V4_LIB.
dnl Arg 2: path to component.
dnl Arg 3: if non-empty, expect and require component.
AC_DEFUN([MOZ_ANDROID_AAR_COMPONENT], [
  ifelse([$3], ,
  [
    if test -e "$$1" ; then
      AC_MSG_ERROR([Found unexpected exploded $1!])
    fi
  ],
  [
    AC_MSG_CHECKING([for $1])
    $1="$2"
    if ! test -e "$$1" ; then
      AC_MSG_ERROR([Could not find required exploded $1!])
    fi
    AC_MSG_RESULT([$$1])
    AC_SUBST($1)
  ])
])

dnl Find an AAR and expose variables representing its exploded components.
dnl AC_SUBSTs ANDROID_NAME_{AAR,AAR_RES,AAR_LIB,AAR_INTERNAL_LIB}.
dnl Arg 1: name, like play-services-base
dnl Arg 2: version, like 7.8.0
dnl Arg 3: extras subdirectory, either android or google
dnl Arg 4: package subdirectory, like com/google/android/gms
dnl Arg 5: if non-empty, expect and require internal_impl JAR.
dnl Arg 6: if non-empty, expect and require assets/ directory.
AC_DEFUN([MOZ_ANDROID_AAR],[
  define([local_aar_var_base], translit($1, [-a-z], [_A-Z]))
  define([local_aar_var], concat(ANDROID_, local_aar_var_base, _AAR))
  local_aar_var="$ANDROID_SDK_ROOT/extras/$3/m2repository/$4/$1/$2/$1-$2.aar"
  AC_MSG_CHECKING([for $1 AAR])
  if ! test -e "$local_aar_var" ; then
    AC_MSG_ERROR([You must download the $1 AAR.  Run the Android SDK tool and install the Android and Google Support Repositories under Extras.  See https://developer.android.com/tools/extras/support-library.html for more info. (Looked for $local_aar_var)])
  fi
  AC_SUBST(local_aar_var)
  AC_MSG_RESULT([$local_aar_var])

  if ! $PYTHON -m mozbuild.action.explode_aar --destdir=$MOZ_BUILD_ROOT/dist/exploded-aar $local_aar_var ; then
    AC_MSG_ERROR([Could not explode $local_aar_var!])
  fi

  define([root], $MOZ_BUILD_ROOT/dist/exploded-aar/$1-$2/)
  MOZ_ANDROID_AAR_COMPONENT(concat(local_aar_var, _LIB), concat(root, $1-$2-classes.jar), REQUIRED)
  MOZ_ANDROID_AAR_COMPONENT(concat(local_aar_var, _RES), concat(root, res), REQUIRED)
  MOZ_ANDROID_AAR_COMPONENT(concat(local_aar_var, _INTERNAL_LIB), concat(root, libs/$1-$2-internal_impl-$2.jar), $5)
  MOZ_ANDROID_AAR_COMPONENT(concat(local_aar_var, _ASSETS), concat(root, assets), $6)
])

AC_DEFUN([MOZ_ANDROID_GOOGLE_PLAY_SERVICES],
[

if test -n "$MOZ_NATIVE_DEVICES" ; then
    AC_SUBST(MOZ_NATIVE_DEVICES)

    MOZ_ANDROID_AAR(play-services-base, 7.8.0, google, com/google/android/gms)
    MOZ_ANDROID_AAR(play-services-cast, 7.8.0, google, com/google/android/gms)
    MOZ_ANDROID_AAR(appcompat-v7, 22.2.1, android, com/android/support)
    MOZ_ANDROID_AAR(mediarouter-v7, 22.2.1, android, com/android/support, REQUIRED_INTERNAL_IMPL)
fi

])

AC_DEFUN([MOZ_ANDROID_SDK],
[

MOZ_ARG_WITH_STRING(android-sdk,
[  --with-android-sdk=DIR
                          location where the Android SDK can be found (base directory, e.g. .../android/platforms/android-6)],
    android_sdk=$withval)

android_sdk_root=${withval%/platforms/android-*}

case "$target" in
*-android*|*-linuxandroid*)
    if test -z "$android_sdk" ; then
        AC_MSG_ERROR([You must specify --with-android-sdk=/path/to/sdk when targeting Android.])
    else
        if ! test -e "$android_sdk"/source.properties ; then
            AC_MSG_ERROR([The path in --with-android-sdk isn't valid (source.properties hasn't been found).])
        fi

        # Get the api level from "$android_sdk"/source.properties.
        ANDROID_TARGET_SDK=`$AWK -F = changequote(<<, >>)'<<$>>1 == "AndroidVersion.ApiLevel" {print <<$>>2}'changequote([, ]) "$android_sdk"/source.properties`

        if test -z "$ANDROID_TARGET_SDK" ; then
            AC_MSG_ERROR([Unexpected error: no AndroidVersion.ApiLevel field has been found in source.properties.])
        fi

	AC_DEFINE_UNQUOTED(ANDROID_TARGET_SDK,$ANDROID_TARGET_SDK)
	AC_SUBST(ANDROID_TARGET_SDK)

        if ! test "$ANDROID_TARGET_SDK" -eq "$ANDROID_TARGET_SDK" ; then
            AC_MSG_ERROR([Unexpected error: the found android api value isn't a number! (found $ANDROID_TARGET_SDK)])
        fi

        if test $ANDROID_TARGET_SDK -lt $1 ; then
            AC_MSG_ERROR([The given Android SDK provides API level $ANDROID_TARGET_SDK ($1 or higher required).])
        fi
    fi

    android_tools="$android_sdk_root"/tools
    android_platform_tools="$android_sdk_root"/platform-tools
    if test ! -d "$android_platform_tools" ; then
        android_platform_tools="$android_sdk"/tools # SDK Tools < r8
    fi

    dnl The build tools got moved around to different directories in SDK
    dnl Tools r22. Try to locate them. This is awful, but, from
    dnl http://stackoverflow.com/a/4495368, the following sorts versions
    dnl of the form x.y.z.a.b from newest to oldest:
    dnl sort -t. -k 1,1nr -k 2,2nr -k 3,3nr -k 4,4nr -k 5,5nr
    dnl We want to favour the newer versions that start with 'android-';
    dnl that's what the sed is about.
    dnl We might iterate over directories that aren't build-tools at all;
    dnl we use the presence of aapt as a marker.
    AC_MSG_CHECKING([for android build-tools directory])
    android_build_tools=""
    for suffix in `ls "$android_sdk_root/build-tools" | sed -e "s,android-,999.," | sort -t. -k 1,1nr -k 2,2nr -k 3,3nr -k 4,4nr -k 5,5nr`; do
        tools_directory=`echo "$android_sdk_root/build-tools/$suffix" | sed -e "s,999.,android-,"`
        if test -d "$tools_directory" -a -f "$tools_directory/aapt"; then
            android_build_tools="$tools_directory"
            break
        fi
    done
    if test -z "$android_build_tools" ; then
        android_build_tools="$android_platform_tools" # SDK Tools < r22
    fi
    all_android_build_tools=""
    for suffix in `ls "$android_sdk_root/build-tools" | sed -e "s,android-,999.," | sort -t. -k 1,1nr -k 2,2nr -k 3,3nr -k 4,4nr -k 5,5nr`; do
        tools_directory=`echo "$android_sdk_root/build-tools/$suffix" | sed -e "s,999.,android-,"`
        if test -d "$tools_directory" -a -f "$tools_directory/aapt"; then
            all_android_build_tools="$all_android_build_tools:$tools_directory"
        fi
    done

    if test -d "$android_build_tools" -a -f "$android_build_tools/aapt"; then
        AC_MSG_RESULT([$android_build_tools])
    else
        AC_MSG_ERROR([not found. Please check your SDK for the subdirectory of build-tools. With the current configuration, it should be in $android_sdk_root/build_tools])
    fi

    ANDROID_SDK="${android_sdk}"
    ANDROID_SDK_ROOT="${android_sdk_root}"

    ANDROID_TOOLS="${android_tools}"
    ANDROID_PLATFORM_TOOLS="${android_platform_tools}"
    ANDROID_BUILD_TOOLS="${android_build_tools}"
    AC_SUBST(ANDROID_SDK_ROOT)
    AC_SUBST(ANDROID_SDK)
    AC_SUBST(ANDROID_TOOLS)
    AC_SUBST(ANDROID_PLATFORM_TOOLS)
    AC_SUBST(ANDROID_BUILD_TOOLS)

    dnl Google has a history of moving the Android tools around.  We don't
    dnl care where they are, so let's try to find them anywhere we can.
    ALL_ANDROID_TOOLS_PATHS="$ANDROID_TOOLS$all_android_build_tools:$ANDROID_PLATFORM_TOOLS"
    MOZ_PATH_PROG(ZIPALIGN, zipalign, :, [$ALL_ANDROID_TOOLS_PATHS])
    MOZ_PATH_PROG(DX, dx, :, [$ALL_ANDROID_TOOLS_PATHS])
    MOZ_PATH_PROG(AAPT, aapt, :, [$ALL_ANDROID_TOOLS_PATHS])
    MOZ_PATH_PROG(AIDL, aidl, :, [$ALL_ANDROID_TOOLS_PATHS])
    MOZ_PATH_PROG(ADB, adb, :, [$ALL_ANDROID_TOOLS_PATHS])

    if test -z "$ZIPALIGN" -o "$ZIPALIGN" = ":"; then
      AC_MSG_ERROR([The program zipalign was not found.  Use --with-android-sdk={android-sdk-dir}.])
    fi
    if test -z "$DX" -o "$DX" = ":"; then
      AC_MSG_ERROR([The program dx was not found.  Use --with-android-sdk={android-sdk-dir}.])
    fi
    if test -z "$AAPT" -o "$AAPT" = ":"; then
      AC_MSG_ERROR([The program aapt was not found.  Use --with-android-sdk={android-sdk-dir}.])
    fi
    if test -z "$AIDL" -o "$AIDL" = ":"; then
      AC_MSG_ERROR([The program aidl was not found.  Use --with-android-sdk={android-sdk-dir}.])
    fi
    if test -z "$ADB" -o "$ADB" = ":"; then
      AC_MSG_ERROR([The program adb was not found.  Use --with-android-sdk={android-sdk-dir}.])
    fi

    MOZ_ANDROID_AAR(support-v4, 22.2.1, android, com/android/support, REQUIRED_INTERNAL_IMPL)
    MOZ_ANDROID_AAR(recyclerview-v7, 22.2.1, android, com/android/support)

    ANDROID_SUPPORT_ANNOTATIONS_JAR="$ANDROID_SDK_ROOT/extras/android/m2repository/com/android/support/support-annotations/22.2.1/support-annotations-22.2.1.jar"
    AC_MSG_CHECKING([for support-annotations JAR])
    if ! test -e $ANDROID_SUPPORT_ANNOTATIONS_JAR ; then
        AC_MSG_ERROR([You must download the support-annotations lib.  Run the Android SDK tool and install the Android Support Repository under Extras.  See https://developer.android.com/tools/extras/support-library.html for more info. (looked for $ANDROID_SUPPORT_ANNOTATIONS_JAR)])
    fi
    AC_MSG_RESULT([$ANDROID_SUPPORT_ANNOTATIONS_JAR])
    AC_SUBST(ANDROID_SUPPORT_ANNOTATIONS_JAR)
    ANDROID_SUPPORT_ANNOTATIONS_JAR_LIB=$ANDROID_SUPPORT_ANNOTATIONS_JAR
    AC_SUBST(ANDROID_SUPPORT_ANNOTATIONS_JAR_LIB)
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

    AC_DEFINE_UNQUOTED(MOZ_ANDROID_MIN_SDK_VERSION, $MOZ_ANDROID_MIN_SDK_VERSION)
    AC_SUBST(MOZ_ANDROID_MIN_SDK_VERSION)
fi

if test -n "$MOZ_ANDROID_MAX_SDK_VERSION"; then
    AC_DEFINE_UNQUOTED(MOZ_ANDROID_MAX_SDK_VERSION, $MOZ_ANDROID_MAX_SDK_VERSION)
    AC_SUBST(MOZ_ANDROID_MAX_SDK_VERSION)
fi

])
