dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl =======================================================================
dnl = Enable compiling with various compiler wrappers (distcc, ccache, etc)
dnl =======================================================================
AC_DEFUN([MOZ_CHECK_COMPILER_WRAPPER],
[
MOZ_ARG_WITH_STRING(compiler_wrapper,
[  --with-compiler-wrapper[=path/to/wrapper]
    Enable compiling with wrappers such as distcc and ccache],
    COMPILER_WRAPPER=$withval, COMPILER_WRAPPER="no")

if test "$COMPILER_WRAPPER" != "no"; then
    CC="$COMPILER_WRAPPER $CC"
    CXX="$COMPILER_WRAPPER $CXX"
    MOZ_USING_COMPILER_WRAPPER=1
fi

AC_SUBST(MOZ_USING_COMPILER_WRAPPER)
])
