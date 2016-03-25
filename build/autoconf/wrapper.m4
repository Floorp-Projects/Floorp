dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl =======================================================================
dnl = Enable compiling with various compiler wrappers (distcc, ccache, etc)
dnl =======================================================================
AC_DEFUN([MOZ_CHECK_COMPILER_WRAPPER],
[
if test -n "$COMPILER_WRAPPER"; then
    case "$CC" in
    $COMPILER_WRAPPER\ *)
        :
        ;;
    *)
        CC="$COMPILER_WRAPPER $CC"
        CXX="$COMPILER_WRAPPER $CXX"
        _SUBDIR_CC="$CC"
        _SUBDIR_CXX="$CXX"
        ac_cv_prog_CC="$CC"
        ac_cv_prog_CXX="$CXX"
        ;;
    esac
fi
])
