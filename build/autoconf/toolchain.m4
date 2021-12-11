dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Several autoconf functions AC_REQUIRE AC_PROG_CPP/AC_PROG_CXXCPP
dnl or AC_HEADER_STDC, meaning they are called even when we don't call
dnl them explicitly.
dnl However, theses checks are not necessary and python configure sets
dnl the corresponding variables already, so just skip those tests
dnl entirely.
define([AC_PROG_CPP],[])
define([AC_PROG_CXXCPP],[])
define([AC_HEADER_STDC], [])

dnl AC_LANG_* set ac_link to the C/C++ compiler, which works fine with
dnl gcc and clang, but not great with clang-cl, where the build system
dnl currently expects to run the linker independently. So LDFLAGS are not
dnl really adapted to be used with clang-cl, which then proceeds to
dnl execute link.exe rather than lld-link.exe.
dnl So when the compiler is clang-cl, we modify ac_link to use a separate
dnl linker call.
define([_MOZ_AC_LANG_C], defn([AC_LANG_C]))
define([AC_LANG_C],
[_MOZ_AC_LANG_C
if test "$CC_TYPE" = "clang-cl"; then
  ac_link="$ac_compile"' && ${LINKER} -OUT:conftest${ac_exeext} $LDFLAGS conftest.obj $LIBS 1>&AC_FD_CC'
fi
])

define([_MOZ_AC_LANG_CPLUSPLUS], defn([AC_LANG_CPLUSPLUS]))
define([AC_LANG_CPLUSPLUS],
[_MOZ_AC_LANG_CPLUSPLUS
if test "$CC_TYPE" = "clang-cl"; then
  ac_link="$ac_compile"' && ${LINKER} -OUT:conftest${ac_exeext} $LDFLAGS conftest.obj $LIBS 1>&AC_FD_CC'
fi
])

AC_DEFUN([MOZ_TOOL_VARIABLES],
[
GNU_CC=
GNU_CXX=
if test "$CC_TYPE" = "gcc"; then
    GNU_CC=1
    GNU_CXX=1
fi

CLANG_CC=
CLANG_CXX=
CLANG_CL=
if test "$CC_TYPE" = "clang"; then
    GNU_CC=1
    GNU_CXX=1
    CLANG_CC=1
    CLANG_CXX=1
fi
if test "$CC_TYPE" = "clang-cl"; then
    CLANG_CL=1
fi

AC_SUBST(CLANG_CXX)
AC_SUBST(CLANG_CL)
])

AC_DEFUN([MOZ_CROSS_COMPILER],
[
echo "cross compiling from $host to $target"

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
AC_PROG_CC
AC_PROG_CXX

AC_CHECK_PROGS(RANLIB, "${TOOLCHAIN_PREFIX}ranlib", :)
AC_CHECK_PROGS(AS, "${TOOLCHAIN_PREFIX}as", :)
AC_CHECK_PROGS(LIPO, "${TOOLCHAIN_PREFIX}lipo", :)
AC_CHECK_PROGS(STRIP, "${TOOLCHAIN_PREFIX}strip", :)
AC_CHECK_PROGS(OTOOL, "${TOOLCHAIN_PREFIX}otool", :)
AC_CHECK_PROGS(INSTALL_NAME_TOOL, "${TOOLCHAIN_PREFIX}install_name_tool", :)
AC_CHECK_PROGS(OBJCOPY, "${TOOLCHAIN_PREFIX}objcopy", :)
PATH=$_SAVE_PATH
])

AC_DEFUN([MOZ_CXX11],
[
dnl Updates to the test below should be duplicated further below for the
dnl cross-compiling case.
AC_LANG_CPLUSPLUS
if test "$GNU_CXX"; then
    AC_CACHE_CHECK([whether 64-bits std::atomic requires -latomic],
        ac_cv_needs_atomic,
        dnl x86 with clang is a little peculiar.  std::atomic does not require
        dnl linking with libatomic, but using atomic intrinsics does, so we
        dnl force the setting on for such systems.
        if test "$CC_TYPE" = "clang" -a "$CPU_ARCH" = "x86" -a "$OS_ARCH" = "Linux"; then
            ac_cv_needs_atomic=yes
        else
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
        fi
    )
    if test "$ac_cv_needs_atomic" = yes; then
      MOZ_NEEDS_LIBATOMIC=1
    else
      MOZ_NEEDS_LIBATOMIC=
    fi
    AC_SUBST(MOZ_NEEDS_LIBATOMIC)
fi
AC_LANG_C
])
