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
COMPILER msvc _MSC_VER
#endif
#elif defined(__clang__)
COMPILER clang __clang_major__.__clang_minor__.__clang_patchlevel__
#elif defined(__GNUC__)
COMPILER gcc __GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__
#elif defined(__INTEL_COMPILER)
COMPILER icc __INTEL_COMPILER
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
INTEL_CC=
INTEL_CXX=
if test "$compiler" = "icc"; then
   INTEL_CC=1
   INTEL_CXX=1
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
fi

if test "$GNU_CC"; then
    if `$CC -print-prog-name=ld` -v 2>&1 | grep -c GNU >/dev/null; then
        GCC_USE_GNU_LD=1
    fi
fi

AC_SUBST(CLANG_CXX)
AC_SUBST(CLANG_CL)
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

if test -z "$HOST_CFLAGS"; then
    HOST_CFLAGS="$CFLAGS"
fi
if test -z "$HOST_CXXFLAGS"; then
    HOST_CXXFLAGS="$CXXFLAGS"
fi
if test -z "$HOST_LDFLAGS"; then
    HOST_LDFLAGS="$LDFLAGS"
fi
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

AC_CHECK_PROGS(CC, "${target_alias}-gcc" "${target}-gcc", :)
unset ac_cv_prog_CC
AC_PROG_CC
AC_CHECK_PROGS(CXX, "${target_alias}-g++" "${target}-g++", :)
unset ac_cv_prog_CXX
AC_PROG_CXX

AC_CHECK_PROGS(RANLIB, "${target_alias}-ranlib" "${target}-ranlib", :)
AC_CHECK_PROGS(AR, "${target_alias}-ar" "${target}-ar", :)
MOZ_PATH_PROGS(AS, "${target_alias}-as" "${target}-as", :)
AC_CHECK_PROGS(LD, "${target_alias}-ld" "${target}-ld", :)
AC_CHECK_PROGS(STRIP, "${target_alias}-strip" "${target}-strip", :)
AC_CHECK_PROGS(WINDRES, "${target_alias}-windres" "${target}-windres", :)
AC_DEFINE(CROSS_COMPILE)
CROSS_COMPILE=1

dnl If we cross compile for ppc on Mac OS X x86, cross_compiling will
dnl dnl have erroneously been set to "no", because the x86 build host is
dnl dnl able to run ppc code in a translated environment, making a cross
dnl dnl compiler appear native.  So we override that here.
cross_compiling=yes
])

AC_DEFUN([MOZ_CXX11],
[
dnl Check whether gcc's c++0x mode works
dnl Updates to the test below should be duplicated further below for the
dnl cross-compiling case.
AC_LANG_CPLUSPLUS
if test "$GNU_CXX"; then
    CXXFLAGS="$CXXFLAGS -std=gnu++0x"
    _ADDED_CXXFLAGS="-std=gnu++0x"

    AC_CACHE_CHECK(for gcc c++0x headers bug without rtti,
        ac_cv_cxx0x_headers_bug,
        [AC_TRY_COMPILE([#include <memory>], [],
                        ac_cv_cxx0x_headers_bug="no",
                        ac_cv_cxx0x_headers_bug="yes")])

    if test "$CLANG_CXX" -a "$ac_cv_cxx0x_headers_bug" = "yes"; then
        CXXFLAGS="$CXXFLAGS -I$_topsrcdir/build/unix/headers"
        _ADDED_CXXFLAGS="$_ADDED_CXXFLAGS -I$_topsrcdir/build/unix/headers"
        AC_CACHE_CHECK(whether workaround for gcc c++0x headers conflict with clang works,
            ac_cv_cxx0x_clang_workaround,
            [AC_TRY_COMPILE([#include <memory>], [],
                            ac_cv_cxx0x_clang_workaround="yes",
                            ac_cv_cxx0x_clang_workaround="no")])

        if test "ac_cv_cxx0x_clang_workaround" = "no"; then
            AC_MSG_ERROR([Your toolchain does not support C++0x/C++11 mode properly. Please upgrade your toolchain])
        fi
    elif test "$ac_cv_cxx0x_headers_bug" = "yes"; then
        AC_MSG_ERROR([Your toolchain does not support C++0x/C++11 mode properly. Please upgrade your toolchain])
    fi
fi
if test -n "$CROSS_COMPILE"; then
    dnl When cross compile, we have no variable telling us what the host compiler is. Figure it out.
    cat > conftest.C <<EOF
#if defined(__clang__)
CLANG
#elif defined(__GNUC__)
GCC
#endif
EOF
    host_compiler=`$HOST_CXX -E conftest.C | egrep '(CLANG|GCC)'`
    rm conftest.C
    if test -n "$host_compiler"; then
        HOST_CXXFLAGS="$HOST_CXXFLAGS -std=gnu++0x"

        _SAVE_CXXFLAGS="$CXXFLAGS"
        _SAVE_CPPFLAGS="$CPPFLAGS"
        _SAVE_CXX="$CXX"
        CXXFLAGS="$HOST_CXXFLAGS"
        CPPFLAGS="$HOST_CPPFLAGS"
        CXX="$HOST_CXX"
        AC_CACHE_CHECK(for host gcc c++0x headers bug without rtti,
            ac_cv_host_cxx0x_headers_bug,
            [AC_TRY_COMPILE([#include <memory>], [],
                            ac_cv_host_cxx0x_headers_bug="no",
                            ac_cv_host_cxx0x_headers_bug="yes")])

        if test "$host_compiler" = CLANG -a "$ac_cv_host_cxx0x_headers_bug" = "yes"; then
            CXXFLAGS="$CXXFLAGS -I$_topsrcdir/build/unix/headers"
            AC_CACHE_CHECK(whether workaround for host gcc c++0x headers conflict with host clang works,
                ac_cv_host_cxx0x_clang_workaround,
                [AC_TRY_COMPILE([#include <memory>], [],
                                ac_cv_host_cxx0x_clang_workaround="yes",
                                ac_cv_host_cxx0x_clang_workaround="no")])

            if test "ac_cv_host_cxx0x_clang_workaround" = "no"; then
                AC_MSG_ERROR([Your host toolchain does not support C++0x/C++11 mode properly. Please upgrade your toolchain])
            fi
            HOST_CXXFLAGS="$CXXFLAGS"
        elif test "$ac_cv_host_cxx0x_headers_bug" = "yes"; then
            AC_MSG_ERROR([Your host toolchain does not support C++0x/C++11 mode properly. Please upgrade your toolchain])
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
