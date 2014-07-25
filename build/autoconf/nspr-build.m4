dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_NSPR], [

ifelse([$1],,define(CONFIGURING_JS,yes))

dnl Possible ways this can be called:
dnl   from toplevel configure:
dnl     JS_STANDALONE=  BUILDING_JS=
dnl   from js/src/configure invoked by toplevel configure:
dnl     JS_STANDALONE=  BUILDING_JS=1
dnl   from standalone js/src/configure:
dnl     JS_STANDALONE=1 BUILDING_JS=1

dnl ========================================================
dnl = Find the right NSPR to use.
dnl ========================================================
MOZ_ARG_WITH_STRING(nspr-cflags,
[  --with-nspr-cflags=FLAGS
                          Pass FLAGS to CC when building code that uses NSPR.
                          Use this when there's no accurate nspr-config
                          script available.  This is the case when building
                          SpiderMonkey as part of the Mozilla tree: the
                          top-level configure script computes NSPR flags
                          that accomodate the quirks of that environment.],
    NSPR_CFLAGS=$withval)
MOZ_ARG_WITH_STRING(nspr-libs,
[  --with-nspr-libs=LIBS   Pass LIBS to LD when linking code that uses NSPR.
                          See --with-nspr-cflags for more details.],
    NSPR_LIBS=$withval)

ifdef([CONFIGURING_JS],[
    MOZ_ARG_ENABLE_BOOL(nspr-build,
[  --enable-nspr-build     Configure and build NSPR from source tree],
        MOZ_BUILD_NSPR=1,
        MOZ_BUILD_NSPR=)
])

if test -z "$BUILDING_JS" || test -n "$JS_STANDALONE"; then
  _IS_OUTER_CONFIGURE=1
fi

MOZ_ARG_WITH_BOOL(system-nspr,
[  --with-system-nspr      Use an NSPR that is already built and installed.
                          Use the 'nspr-config' script in the current path,
                          or look for the script in the directories given with
                          --with-nspr-exec-prefix or --with-nspr-prefix.
                          (Those flags are only checked if you specify
                          --with-system-nspr.)],
    _USE_SYSTEM_NSPR=1 )

JS_POSIX_NSPR=unset
ifdef([CONFIGURING_JS],[
    if test -n "$JS_STANDALONE"; then
      case "$target" in
        *linux*|*darwin*|*dragonfly*|*freebsd*|*netbsd*|*openbsd*)
          if test -z "$_HAS_NSPR"; then
            JS_POSIX_NSPR_DEFAULT=1
          fi
          ;;
      esac
    fi

    MOZ_ARG_ENABLE_BOOL(posix-nspr-emulation,
[  --enable-posix-nspr-emulation
                          Enable emulation of NSPR for POSIX systems],
    JS_POSIX_NSPR=1,
    JS_POSIX_NSPR=)
])

dnl Pass at most one of
dnl   --with-system-nspr
dnl   --with-nspr-cflags/libs
dnl   --enable-nspr-build
dnl   --enable-posix-nspr-emulation

AC_MSG_CHECKING([NSPR selection])
nspr_opts=
which_nspr=default
if test -n "$_USE_SYSTEM_NSPR"; then
    nspr_opts="x$nspr_opts"
    which_nspr="system"
fi
if test -n "$NSPR_CFLAGS" -o -n "$NSPR_LIBS"; then
    nspr_opts="x$nspr_opts"
    which_nspr="command-line"
fi
if test -n "$MOZ_BUILD_NSPR"; then
    nspr_opts="x$nspr_opts"
    which_nspr="source-tree"
fi
if test "$JS_POSIX_NSPR" = unset; then
    JS_POSIX_NSPR=
else
    nspr_opts="x$nspr_opts"
    which_nspr="posix-wrapper"
fi

if test -z "$nspr_opts"; then
    if test -z "$BUILDING_JS"; then
      dnl Toplevel configure defaults to using nsprpub from the source tree
      MOZ_BUILD_NSPR=1
      which_nspr="source-tree"
    else
      dnl JS configure defaults to emulated NSPR if available, falling back
      dnl to nsprpub.
      JS_POSIX_NSPR="$JS_POSIX_NSPR_DEFAULT"
      if test -z "$JS_POSIX_NSPR"; then
        MOZ_BUILD_NSPR=1
        which_nspr="source-tree"
      else
        which_nspr="posix-wrapper"
      fi
   fi
fi

if test -z "$nspr_opts" || test "$nspr_opts" = x; then
    AC_MSG_RESULT($which_nspr)
else
    AC_MSG_ERROR([only one way of using NSPR may be selected. See 'configure --help'.])
fi

AC_SUBST(MOZ_BUILD_NSPR)

if test -n "$BUILDING_JS"; then
  if test "$JS_POSIX_NSPR" = 1; then
    AC_DEFINE(JS_POSIX_NSPR)
  fi
  AC_SUBST(JS_POSIX_NSPR)
fi

# A (sub)configure invoked by the toplevel configure will always receive
# --with-nspr-libs on the command line. It will never need to figure out
# anything itself.
if test -n "$_IS_OUTER_CONFIGURE"; then

if test -n "$_USE_SYSTEM_NSPR"; then
    AM_PATH_NSPR($NSPR_MINVER, [MOZ_NATIVE_NSPR=1], [AC_MSG_ERROR([you do not have NSPR installed or your version is older than $NSPR_MINVER.])])
fi

if test -n "$MOZ_NATIVE_NSPR" -o -n "$NSPR_CFLAGS" -o -n "$NSPR_LIBS"; then
    _SAVE_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS $NSPR_CFLAGS"
    AC_TRY_COMPILE([#include "prtypes.h"],
                [#ifndef PR_STATIC_ASSERT
                 #error PR_STATIC_ASSERT not defined or requires including prtypes.h
                 #endif],
                ,
                AC_MSG_ERROR([system NSPR does not support PR_STATIC_ASSERT or including prtypes.h does not provide it]))
    AC_TRY_COMPILE([#include "prtypes.h"],
                [#ifndef PR_UINT64
                 #error PR_UINT64 not defined or requires including prtypes.h
                 #endif],
                ,
                AC_MSG_ERROR([system NSPR does not support PR_UINT64 or including prtypes.h does not provide it]))
    CFLAGS=$_SAVE_CFLAGS
elif test -z "$JS_POSIX_NSPR"; then
    if test -z "$LIBXUL_SDK"; then
        NSPR_CFLAGS="-I${LIBXUL_DIST}/include/nspr"
        if test -n "$GNU_CC"; then
            NSPR_LIBS="-L${LIBXUL_DIST}/lib -lnspr${NSPR_VERSION} -lplc${NSPR_VERSION} -lplds${NSPR_VERSION}"
        else
            NSPR_LIBS="${LIBXUL_DIST}/lib/nspr${NSPR_VERSION}.lib ${LIBXUL_DIST}/lib/plc${NSPR_VERSION}.lib ${LIBXUL_DIST}/lib/plds${NSPR_VERSION}.lib "
        fi
    else
        NSPR_CFLAGS=`"${LIBXUL_DIST}"/sdk/bin/nspr-config --prefix="${LIBXUL_DIST}" --includedir="${LIBXUL_DIST}/include/nspr" --cflags`
        NSPR_LIBS=`"${LIBXUL_DIST}"/sdk/bin/nspr-config --prefix="${LIBXUL_DIST}" --libdir="${LIBXUL_DIST}"/lib --libs`
    fi
fi

AC_SUBST(NSPR_CFLAGS)
AC_SUBST(NSPR_LIBS)

NSPR_PKGCONF_CHECK="nspr"
if test -n "$MOZ_NATIVE_NSPR"; then
    # piggy back on $MOZ_NATIVE_NSPR to set a variable for the nspr check for js.pc
    NSPR_PKGCONF_CHECK="nspr >= $NSPR_MINVER"

    _SAVE_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS $NSPR_CFLAGS"
    AC_TRY_COMPILE([#include "prlog.h"],
                [#ifndef PR_STATIC_ASSERT
                 #error PR_STATIC_ASSERT not defined
                 #endif],
                ,
                AC_MSG_ERROR([system NSPR does not support PR_STATIC_ASSERT]))
    CFLAGS=$_SAVE_CFLAGS
fi
AC_SUBST(NSPR_PKGCONF_CHECK)

fi # _IS_OUTER_CONFIGURE

])

AC_DEFUN([MOZ_SUBCONFIGURE_NSPR], [

if test -z "$MOZ_NATIVE_NSPR"; then
    ac_configure_args="$_SUBDIR_CONFIG_ARGS --with-dist-prefix=$MOZ_BUILD_ROOT/dist --with-mozilla"
    if test -z "$MOZ_DEBUG"; then
        ac_configure_args="$ac_configure_args --disable-debug"
    else
        ac_configure_args="$ac_configure_args --enable-debug"
        if test -n "$MOZ_NO_DEBUG_RTL"; then
            ac_configure_args="$ac_configure_args --disable-debug-rtl"
        fi
    fi
    if test "$MOZ_OPTIMIZE" = "1"; then
        ac_configure_args="$ac_configure_args --enable-optimize"
    elif test -z "$MOZ_OPTIMIZE"; then
        ac_configure_args="$ac_configure_args --disable-optimize"
    fi
    if test -n "$HAVE_64BIT_BUILD"; then
        ac_configure_args="$ac_configure_args --enable-64bit"
    fi
    if test -n "$USE_ARM_KUSER"; then
        ac_configure_args="$ac_configure_args --with-arm-kuser"
    fi
    # A configure script generated by autoconf 2.68 does not allow the cached
    # values of "precious" variables such as CFLAGS and LDFLAGS to differ from
    # the values passed to the configure script. Since we modify CFLAGS and
    # LDFLAGS before passing them to NSPR's configure script, we cannot share
    # config.cache with NSPR. As a result, we cannot pass AS, CC, CXX, etc. to
    # NSPR via a shared config.cache file and must pass them to NSPR on the
    # configure command line.
    for var in AS CC CXX CPP LD AR RANLIB STRIP; do
        ac_configure_args="$ac_configure_args $var='`eval echo \\${${var}}`'"
    done
    # A configure script generated by autoconf 2.68 warns if --host is
    # specified but --build isn't. So we always pass --build to NSPR's
    # configure script.
    ac_configure_args="$ac_configure_args --build=$build"
    ac_configure_args="$ac_configure_args $NSPR_CONFIGURE_ARGS"

    # Save these, so we can mess with them for the subconfigure ..
    _SAVE_CFLAGS="$CFLAGS"
    _SAVE_CPPFLAGS="$CPPFLAGS"
    _SAVE_LDFLAGS="$LDFLAGS"

    if test -n "$MOZ_LINKER" -a "$ac_cv_func_dladdr" = no ; then
      # dladdr is supported by the new linker, even when the system linker doesn't
      # support it. Trick nspr into using dladdr when it's not supported.
      export CPPFLAGS="-include $_topsrcdir/mozglue/linker/dladdr.h $CPPFLAGS"
    fi
    export LDFLAGS="$LDFLAGS $NSPR_LDFLAGS"
    export CFLAGS="$CFLAGS $MOZ_FRAMEPTR_FLAGS"

    AC_OUTPUT_SUBDIRS(nsprpub)

    # .. and restore them
    CFLAGS="$_SAVE_CFLAGS"
    CPPFLAGS="$_SAVE_CPPFLAGS"
    LDFLAGS="$_SAVE_LDFLAGS"

    ac_configure_args="$_SUBDIR_CONFIG_ARGS"
fi

])
