dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_TOOL_VARIABLES],
[
GNU_AS=
GNU_LD=
GNU_CC=
GNU_CXX=
CC_VERSION='N/A'
CXX_VERSION='N/A'
cat <<EOF > conftest.c
#if defined(_MSC_VER)
#if defined(__clang__)
COMPILER clang-cl _MSC_VER
#else
COMPILER msvc _MSC_FULL_VER
#endif
#elif defined(__clang__)
COMPILER clang __clang_major__.__clang_minor__.__clang_patchlevel__
#elif defined(__GNUC__)
COMPILER gcc __GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__
#endif
EOF
read dummy compiler CC_VERSION <<EOF
$($CC -E $CPPFLAGS $CFLAGS conftest.c 2>/dev/null | grep COMPILER)
EOF
read dummy cxxcompiler CXX_VERSION <<EOF
$($CXX -E $CPPFLAGS $CXXFLAGS conftest.c 2>/dev/null | grep COMPILER)
EOF
if test "$compiler" != "$cxxcompiler"; then
    AC_MSG_ERROR([Your C and C++ compilers are different.  You need to use the same compiler.])
fi
if test "$CC_VERSION" != "$CXX_VERSION"; then
    # This may not be strictly necessary, but if we want to drop it, we
    # should make sure any version checks below apply to both the C and
    # C++ compiler versions.
    AC_MSG_ERROR([Your C and C++ compiler versions are different.  You need to use the same compiler version.])
fi
CC_VERSION=`echo "$CC_VERSION" | sed 's/ //g'`
CXX_VERSION=`echo "$CXX_VERSION" | sed 's/ //g'`
if test "$compiler" = "gcc"; then
    GNU_CC=1
    GNU_CXX=1
    changequote(<<,>>)
    GCC_VERSION_FULL="$CXX_VERSION"
    GCC_VERSION=`echo "$GCC_VERSION_FULL" | $PERL -pe '(split(/\./))[0]>=4&&s/(^\d*\.\d*).*/<<$>>1/;'`

    GCC_MAJOR_VERSION=`echo ${GCC_VERSION} | $AWK -F\. '{ print <<$>>1 }'`
    GCC_MINOR_VERSION=`echo ${GCC_VERSION} | $AWK -F\. '{ print <<$>>2 }'`
    changequote([,])
fi

if test "`echo | $AS -o conftest.out -v 2>&1 | grep -c GNU`" != "0"; then
    GNU_AS=1
fi
rm -f conftest.out
if test "`echo | $LD -v 2>&1 | grep -c GNU`" != "0"; then
    GNU_LD=1
fi

if test "$compiler" = "msvc"; then
     MSVC_VERSION_FULL="$CXX_VERSION"
     CC_VERSION=`echo ${CC_VERSION} | cut -c 1-4`
     CXX_VERSION=`echo ${CXX_VERSION} | cut -c 1-4`
fi

CLANG_CC=
CLANG_CXX=
CLANG_CL=
if test "$compiler" = "clang"; then
    GNU_CC=1
    GNU_CXX=1
    CLANG_CC=1
    CLANG_CXX=1
fi
if test "$compiler" = "clang-cl"; then
    CLANG_CL=1
    # We force clang-cl to emulate Visual C++ 2013 in configure.in, but that
    # is based on the CLANG_CL variable defined here, so make sure that we're
    # getting the right version here manually.
    CC_VERSION=1800
    CXX_VERSION=1800
    MSVC_VERSION_FULL=180030723
    # Build on clang-cl with MSVC 2013 Update 3 with fallback emulation.
    CFLAGS="$CFLAGS -fms-compatibility-version=18.00.30723 -fallback"
    CXXFLAGS="$CXXFLAGS -fms-compatibility-version=18.00.30723 -fallback"
fi

if test "$GNU_CC"; then
    if `$CC -print-prog-name=ld` -v 2>&1 | grep -c GNU >/dev/null; then
        GCC_USE_GNU_LD=1
    fi
fi

AC_SUBST(CLANG_CXX)
AC_SUBST(CLANG_CL)

if test -n "$GNU_CC" -a -z "$CLANG_CC" ; then
    if test "$GCC_MAJOR_VERSION" -eq 4 -a "$GCC_MINOR_VERSION" -lt 8 ||
       test "$GCC_MAJOR_VERSION" -lt 4; then
        AC_MSG_ERROR([Only GCC 4.8 or newer supported])
    fi
fi
])

AC_DEFUN([MOZ_CROSS_COMPILER],
[
echo "cross compiling from $host to $target"

_SAVE_CC="$CC"
_SAVE_CFLAGS="$CFLAGS"
_SAVE_LDFLAGS="$LDFLAGS"

AC_MSG_CHECKING([for host c compiler])
AC_CHECK_PROGS(HOST_CC, cc gcc clang cl, "")
if test -z "$HOST_CC"; then
    AC_MSG_ERROR([no acceptable c compiler found in \$PATH])
fi
AC_MSG_RESULT([$HOST_CC])
AC_MSG_CHECKING([for host c++ compiler])
AC_CHECK_PROGS(HOST_CXX, c++ g++ clang++ cl, "")
if test -z "$HOST_CXX"; then
    AC_MSG_ERROR([no acceptable c++ compiler found in \$PATH])
fi
AC_MSG_RESULT([$HOST_CXX])

if test -z "$HOST_AR_FLAGS"; then
    HOST_AR_FLAGS="$AR_FLAGS"
fi
AC_CHECK_PROGS(HOST_RANLIB, $HOST_RANLIB ranlib, ranlib, :)
AC_CHECK_PROGS(HOST_AR, $HOST_AR ar, ar, :)
CC="$HOST_CC"
CFLAGS="$HOST_CFLAGS"
LDFLAGS="$HOST_LDFLAGS"

AC_MSG_CHECKING([whether the host c compiler ($HOST_CC $HOST_CFLAGS $HOST_LDFLAGS) works])
AC_TRY_COMPILE([], [return(0);],
    [ac_cv_prog_hostcc_works=1 AC_MSG_RESULT([yes])],
    AC_MSG_ERROR([installation or configuration problem: host compiler $HOST_CC cannot create executables.]) )

CC="$HOST_CXX"
CFLAGS="$HOST_CXXFLAGS"
AC_MSG_CHECKING([whether the host c++ compiler ($HOST_CXX $HOST_CXXFLAGS $HOST_LDFLAGS) works])
AC_TRY_COMPILE([], [return(0);],
    [ac_cv_prog_hostcxx_works=1 AC_MSG_RESULT([yes])],
    AC_MSG_ERROR([installation or configuration problem: host compiler $HOST_CXX cannot create executables.]) )

CC=$_SAVE_CC
CFLAGS=$_SAVE_CFLAGS
LDFLAGS=$_SAVE_LDFLAGS

dnl AC_CHECK_PROGS manually goes through $PATH, and as such fails to handle
dnl absolute or relative paths. Relative paths wouldn't work anyways, but
dnl absolute paths would. Trick AC_CHECK_PROGS into working in that case by
dnl adding / to PATH. This is temporary until this moves to moz.configure
dnl (soon).
_SAVE_PATH=$PATH
case "${TOOLCHAIN_PREFIX}" in
/*)
    PATH="/:$PATH"
    ;;
esac
AC_CHECK_PROGS(CC, "${TOOLCHAIN_PREFIX}gcc", :)
unset ac_cv_prog_CC
AC_PROG_CC
AC_CHECK_PROGS(CXX, "${TOOLCHAIN_PREFIX}g++", :)
unset ac_cv_prog_CXX
AC_PROG_CXX

AC_CHECK_PROGS(RANLIB, "${TOOLCHAIN_PREFIX}ranlib", :)
AC_CHECK_PROGS(AR, "${TOOLCHAIN_PREFIX}ar", :)
AC_CHECK_PROGS(AS, "${TOOLCHAIN_PREFIX}as", :)
AC_CHECK_PROGS(LD, "${TOOLCHAIN_PREFIX}ld", :)
AC_CHECK_PROGS(STRIP, "${TOOLCHAIN_PREFIX}strip", :)
AC_CHECK_PROGS(WINDRES, "${TOOLCHAIN_PREFIX}windres", :)
AC_CHECK_PROGS(OTOOL, "${TOOLCHAIN_PREFIX}otool", :)
AC_CHECK_PROGS(OBJCOPY, "${TOOLCHAIN_PREFIX}objcopy", :)
PATH=$_SAVE_PATH
])

AC_DEFUN([MOZ_CXX11],
[
dnl Updates to the test below should be duplicated further below for the
dnl cross-compiling case.
AC_LANG_CPLUSPLUS
if test "$GNU_CXX"; then
    CXXFLAGS="$CXXFLAGS -std=gnu++11"
    _ADDED_CXXFLAGS="-std=gnu++11"

    if test -n "$CLANG_CC"; then
        dnl We'd normally just check for the version from CC_VERSION (fed
        dnl from __clang_major__ and __clang_minor__), but the clang that
        dnl comes with Xcode has a completely different version scheme
        dnl despite exposing the version with the same defines.
        dnl So instead of a version check, do a feature check. Normally,
        dnl we'd use __has_feature, but there are unfortunately no C++11
        dnl differences in clang 3.4. However, it supports the 2013-08-28
        dnl draft of the ISO WG21 SG10 feature test macro recommendations.
        AC_TRY_COMPILE([], [#if !__cpp_static_assert
                            #error ISO WG21 SG10 feature test macros unsupported
                            #endif],,AC_MSG_ERROR([Only clang/llvm 3.4 or newer supported]))
    fi

    AC_CACHE_CHECK([whether 64-bits std::atomic requires -latomic],
        ac_cv_needs_atomic,
        AC_TRY_LINK(
            [#include <cstdint>
             #include <atomic>],
            [ std::atomic<uint64_t> foo; foo = 1; ],
            ac_cv_needs_atomic=no,
            _SAVE_LIBS="$LIBS"
            LIBS="$LIBS -latomic"
            AC_TRY_LINK(
                [#include <cstdint>
                 #include <atomic>],
                [ std::atomic<uint64_t> foo; foo = 1; ],
                ac_cv_needs_atomic=yes,
                ac_cv_needs_atomic="do not know; assuming no")
            LIBS="$_SAVE_LIBS"
        )
    )
    if test "$ac_cv_needs_atomic" = yes; then
      MOZ_NEEDS_LIBATOMIC=1
    else
      MOZ_NEEDS_LIBATOMIC=
    fi
    AC_SUBST(MOZ_NEEDS_LIBATOMIC)
fi

if test -n "$CROSS_COMPILE"; then
    dnl When cross compile, we have no variable telling us what the host compiler is. Figure it out.
    cat > conftest.C <<EOF
#if defined(__clang__)
COMPILER CLANG __clang_major__.__clang_minor__.__clang_patchlevel__
#elif defined(__GNUC__)
COMPILER GCC __GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__
#endif
EOF
read dummy host_compiler HOST_CC_VERSION <<EOF
$($HOST_CC -E conftest.C 2>/dev/null | grep COMPILER)
EOF
read dummy host_cxxcompiler HOST_CXX_VERSION <<EOF
$($HOST_CXX -E conftest.C 2>/dev/null | grep COMPILER)
EOF
    rm conftest.C
    if test "$host_compiler" != "$host_cxxcompiler"; then
        AC_MSG_ERROR([Your C and C++ host compilers are different.  You need to use the same compiler.])
    fi
    if test "$HOST_CC_VERSION" != "$HOST_CXX_VERSION"; then
        # This may not be strictly necessary, but if we want to drop it,
        # we should make sure any version checks below apply to both the
        # C and C++ compiler versions.
        AC_MSG_ERROR([Your C and C++ host compiler versions are different.  You need to use the same compiler version.])
    fi
    if test -n "$host_compiler"; then
        if test "$host_compiler" = "GCC" ; then
            changequote(<<,>>)
            HOST_GCC_VERSION_FULL="$HOST_CXX_VERSION"
            HOST_GCC_VERSION=`echo "$HOST_GCC_VERSION_FULL" | $PERL -pe '(split(/\./))[0]>=4&&s/(^\d*\.\d*).*/<<$>>1/;'`

            HOST_GCC_MAJOR_VERSION=`echo ${HOST_GCC_VERSION} | $AWK -F\. '{ print <<$>>1 }'`
            HOST_GCC_MINOR_VERSION=`echo ${HOST_GCC_VERSION} | $AWK -F\. '{ print <<$>>2 }'`
            changequote([,])

            if test "$HOST_GCC_MAJOR_VERSION" -eq 4 -a "$HOST_GCC_MINOR_VERSION" -lt 8 ||
               test "$HOST_GCC_MAJOR_VERSION" -lt 4; then
                AC_MSG_ERROR([Only GCC 4.8 or newer supported for host compiler])
            fi
        fi

        HOST_CXXFLAGS="$HOST_CXXFLAGS -std=gnu++11"

        _SAVE_CXXFLAGS="$CXXFLAGS"
        _SAVE_CPPFLAGS="$CPPFLAGS"
        _SAVE_CXX="$CXX"
        CXXFLAGS="$HOST_CXXFLAGS"
        CPPFLAGS="$HOST_CPPFLAGS"
        CXX="$HOST_CXX"
        if test "$host_compiler" = CLANG; then
            AC_TRY_COMPILE([], [#if !__cpp_static_assert
                                #error ISO WG21 SG10 feature test macros unsupported
                                #endif],,AC_MSG_ERROR([Only clang/llvm 3.4 or newer supported]))
        fi

        CXXFLAGS="$_SAVE_CXXFLAGS"
        CPPFLAGS="$_SAVE_CPPFLAGS"
        CXX="$_SAVE_CXX"
    fi
elif test "$GNU_CXX"; then
    HOST_CXXFLAGS="$HOST_CXXFLAGS $_ADDED_CXXFLAGS"
fi
AC_LANG_C
])
