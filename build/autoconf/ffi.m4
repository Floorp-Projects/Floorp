dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_FFI], [

MOZ_ARG_ENABLE_BOOL(system-ffi,
[  --enable-system-ffi       Use system libffi (located with pkgconfig)],
    MOZ_NATIVE_FFI=1 )

if test -n "$MOZ_NATIVE_FFI"; then
    # Vanilla libffi 3.0.9 needs a few patches from upcoming version 3.0.10
    # for non-GCC compilers.
    if test -z "$GNU_CC"; then
        PKG_CHECK_MODULES(MOZ_FFI, libffi > 3.0.9)
    else
        PKG_CHECK_MODULES(MOZ_FFI, libffi >= 3.0.9)
    fi
fi

AC_SUBST(MOZ_NATIVE_FFI)

])

AC_DEFUN([MOZ_SUBCONFIGURE_FFI], [
if test -z "$BUILDING_JS" -o -n "$JS_STANDALONE"; then

  if test "$BUILD_CTYPES" -a -z "$MOZ_NATIVE_FFI"; then
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
    if test "$CROSS_COMPILE"; then
      export CPPFLAGS CFLAGS LDFLAGS
    fi
    ac_configure_args="$ac_configure_args --build=$build --host=$target"
    if test "$_MSC_VER"; then
      # Use a wrapper script for cl and ml that looks more like gcc.
      # autotools can't quite handle an MSVC build environment yet.
      LDFLAGS=
      CFLAGS=
      ac_configure_args="$ac_configure_args LD=link CPP=\"cl -nologo -EP\" \
                         CXXCPP=\"cl -nologo -EP\" SHELL=sh.exe"
      rtl=
      if test -z "$MOZ_NO_DEBUG_RTL" -a -n "$MOZ_DEBUG"; then
        rtl=" -DUSE_DEBUG_RTL"
      fi
      case "${target_cpu}" in
      x86_64)
        # Need target since MSYS tools into mozilla-build may be 32bit
        ac_configure_args="$ac_configure_args \
                           CC=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh -m64$rtl\" \
                           CXX=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh -m64$rtl\""
        ;;
      *)
        ac_configure_args="$ac_configure_args \
                           CC=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh$rtl\" \
                           CXX=\"$_topsrcdir/js/src/ctypes/libffi/msvcc.sh$rtl\""
        ;;
      esac
    fi
    if test "$SOLARIS_SUNPRO_CC"; then
      # Always use gcc for libffi on Solaris
      if test ! "$HAVE_64BIT_OS"; then
        ac_configure_args="$ac_configure_args CC=gcc CFLAGS=-m32 LD= LDFLAGS="
      else
        ac_configure_args="$ac_configure_args CC=gcc CFLAGS=-m64 LD= LDFLAGS="
      fi
    fi
    if test "$AIX_IBM_XLC"; then
      # Always use gcc for libffi on IBM AIX5/AIX6
      if test ! "$HAVE_64BIT_OS"; then
        ac_configure_args="$ac_configure_args CC=gcc CFLAGS=-maix32"
      else
        ac_configure_args="$ac_configure_args CC=gcc CFLAGS=-maix64"
      fi
    fi

    # Use a separate cache file for libffi, since it does things differently
    # from our configure.
    mkdir -p $_objdir/js/src/ctypes/libffi
    old_cache_file=$cache_file
    cache_file=$_objdir/js/src/ctypes/libffi/config.cache
    old_config_files=$CONFIG_FILES
    unset CONFIG_FILES
    AC_OUTPUT_SUBDIRS(js/src/ctypes/libffi)
    cache_file=$old_cache_file
    ac_configure_args="$_SUBDIR_CONFIG_ARGS"
    CONFIG_FILES=$old_config_files
  fi

fi
])

