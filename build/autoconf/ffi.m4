dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_FFI], [

MOZ_ARG_ENABLE_BOOL(system-ffi,
[  --enable-system-ffi       Use system libffi (located with pkgconfig)],
    MOZ_SYSTEM_FFI=1 )

if test -n "$MOZ_SYSTEM_FFI"; then
    # Vanilla libffi 3.0.9 needs a few patches from upcoming version 3.0.10
    # for non-GCC compilers.
    if test -z "$GNU_CC"; then
        PKG_CHECK_MODULES(MOZ_FFI, libffi > 3.0.9)
    else
        PKG_CHECK_MODULES(MOZ_FFI, libffi >= 3.0.9)
    fi
fi

AC_SUBST(MOZ_SYSTEM_FFI)

])

AC_DEFUN([MOZ_SUBCONFIGURE_FFI], [
if test "$MOZ_BUILD_APP" != js -o -n "$JS_STANDALONE"; then

  if test "$BUILD_CTYPES" -a -z "$MOZ_SYSTEM_FFI"; then
    # Run the libffi 'configure' script.
    ac_configure_args="--disable-shared --enable-static --disable-raw-api"
    if test "$MOZ_DEBUG"; then
      ac_configure_args="$ac_configure_args --enable-debug"
    fi
    if test "$DSO_PIC_CFLAGS"; then
      ac_configure_args="$ac_configure_args --with-pic"
    fi
    for var in AS CC CXX CPP LD AR RANLIB STRIP; do
      ac_configure_args="$ac_configure_args $var='`eval echo \\${${var}}`'"
    done
    old_cflags="$CFLAGS"
    # The libffi sources (especially the ARM ones) are written expecting gas
    # syntax, and clang's integrated assembler doesn't handle all of gas syntax.
    if test -n "$CLANG_CC" -a "$CPU_ARCH" = arm; then
      CFLAGS="-no-integrated-as $CFLAGS"
    fi
    ac_configure_args="$ac_configure_args --build=$build --host=$target"
    if test "$CROSS_COMPILE"; then
      ac_configure_args="$ac_configure_args \
                         CFLAGS=\"$CFLAGS\" \
                         CPPFLAGS=\"$CPPFLAGS\" \
                         LDFLAGS=\"$LDFLAGS\""
    fi
    CFLAGS="$old_cflags"
    if test "$_MSC_VER"; then
      # Use a wrapper script for cl and ml that looks more like gcc.
      # autotools can't quite handle an MSVC build environment yet.
      LDFLAGS=
      CFLAGS=
      ac_configure_args="$ac_configure_args LD=link CPP=\"$CC -nologo -EP\" \
                         CXXCPP=\"$CXX -nologo -EP\" SHELL=sh.exe"
      flags=
      if test -z "$MOZ_NO_DEBUG_RTL" -a -n "$MOZ_DEBUG"; then
        flags=" -DUSE_DEBUG_RTL"
      fi
      if test -n "$CLANG_CL"; then
        flags="$flags -clang-cl"
      fi
      case "${target_cpu}" in
      x86_64)
        # Need target since MSYS tools into mozilla-build may be 32bit
        ac_configure_args="$ac_configure_args \
                           CC=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh -m64$flags\" \
                           CXX=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh -m64$flags\""
        ;;
      *)
        ac_configure_args="$ac_configure_args \
                           CC=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh$flags\" \
                           CXX=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh$flags\""
        ;;
      esac
    fi

    # Use a separate cache file for libffi, since it does things differently
    # from our configure.
    old_config_files=$CONFIG_FILES
    unset CONFIG_FILES
    AC_OUTPUT_SUBDIRS(js/src/ctypes/libffi)
    ac_configure_args="$_SUBDIR_CONFIG_ARGS"
    CONFIG_FILES=$old_config_files
  fi

fi
])

