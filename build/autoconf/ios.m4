dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_IOS_PATH_PROG],
[
changequote({,})
_prog_name=ifelse($2, {}, `echo $1 | tr "[:upper:]" "[:lower:]"`, $2)
changequote([,])
AC_CACHE_CHECK([for $_prog_name in iOS SDK],
ac_cv_ios_path_$1,
[
_path=`xcrun --sdk $ios_sdk --find $_prog_name 2>/dev/null`
_res=$?
if test $_res -ne 0; then
   AC_MSG_ERROR([Could not find '$_prog_name' in the iOS SDK])
fi
ac_cv_ios_path_$1=$_path
])
$1="${ac_cv_ios_path_$1}$3"
])

AC_DEFUN([MOZ_IOS_SDK],
[

MOZ_ARG_WITH_STRING(ios-sdk,
[  --with-ios-sdk=TYPE
                          Type of iOS SDK to use (iphonesimulator, iphoneos)
                          and optionally version (like iphoneos8.2)],
    ios_sdk=$withval)

MOZ_ARG_ENABLE_STRING(ios-target,
                      [  --enable-ios-target=VER (default=8.0)
                          Set the minimum iOS version needed at runtime],
                      [_IOS_TARGET=$enableval])
_IOS_TARGET_DEFAULT=8.0

case "$target" in
arm*-apple-darwin*)
    if test -z "$ios_sdk" -o "$ios_sdk" = "yes"; then
       ios_sdk=iphoneos
    fi
    case "$ios_sdk" in
         iphoneos*)
                ios_target_arg="-miphoneos-version-min"
                ;;
         *)
                AC_MSG_ERROR([Only 'iphoneos' SDKs are valid when targeting iOS device, don't know what to do with '$ios_sdk'.])
                ;;
    esac
    ;;
*-apple-darwin*)
    ios_target_arg="-mios-simulator-version-min"
    case "$ios_sdk" in
         # Empty SDK is okay, this might be an OS X desktop build.
         ""|iphonesimulator*)
                ;;
         # Default to iphonesimulator
         yes)
                ios_sdk=iphonesimulator
                ;;
         *)
                AC_MSG_ERROR([Only 'iphonesimulator' SDKs are valid when targeting iOS simulator.])
                ;;
    esac
    ;;
esac


if test -n "$ios_sdk"; then
   if test -z "$_IOS_TARGET"; then
      _IOS_TARGET=$_IOS_TARGET_DEFAULT
      ios_target_arg="${ios_target_arg}=${_IOS_TARGET}"
   fi
   # Ensure that xcrun knows where this SDK is.
   ios_sdk_path=`xcrun --sdk $ios_sdk --show-sdk-path 2>/dev/null`
   _ret=$?
   if test $_ret -ne 0; then
      AC_MSG_ERROR([iOS SDK '$ios_sdk' could not be found.])
   fi
   MOZ_IOS=1
   export HOST_CC=clang
   export HOST_CXX=clang++
   # Add isysroot, arch, and ios target arguments
   case "$target_cpu" in
        arm*)
                ARGS="-arch armv7"
                ;;
        *)
                # Unfortunately simulator builds need this.
                export CROSS_COMPILE=1
                ;;
   esac
   ARGS=" $ARGS -isysroot $ios_sdk_path $ios_target_arg"
   # Now find our tools
   MOZ_IOS_PATH_PROG(CC, clang, $ARGS)
   MOZ_IOS_PATH_PROG(CXX, clang++, $ARGS)
   export CPP="$CC -E"
   MOZ_IOS_PATH_PROG(AR)
   MOZ_IOS_PATH_PROG(AS, as, $ARGS)
   MOZ_IOS_PATH_PROG(OTOOL)
   MOZ_IOS_PATH_PROG(STRIP)
   export PKG_CONFIG_PATH=${ios_sdk_path}/usr/lib/pkgconfig/
fi

AC_SUBST(MOZ_IOS)
])
