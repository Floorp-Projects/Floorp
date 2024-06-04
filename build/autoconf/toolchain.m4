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

PATH=$_SAVE_PATH
])
