dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl ======================================================
dnl = Enable compiling with ccache
dnl ======================================================
AC_DEFUN([MOZ_CHECK_CCACHE],
[
MOZ_ARG_WITH_STRING(ccache,
[  --with-ccache[=path/to/ccache]
                          Enable compiling with ccache],
    CCACHE=$withval, CCACHE="no")

if test "$CCACHE" != "no"; then
    if test -z "$CCACHE" -o "$CCACHE" = "yes"; then
        CCACHE=
    else
        if test ! -e "$CCACHE"; then
            AC_MSG_ERROR([$CCACHE not found])
        fi
    fi
    MOZ_PATH_PROGS(CCACHE, $CCACHE ccache)
    if test -z "$CCACHE" -o "$CCACHE" = ":"; then
        AC_MSG_ERROR([ccache not found])
    elif test -x "$CCACHE"; then
        CC="$CCACHE $CC"
        CXX="$CCACHE $CXX"
        MOZ_USING_CCACHE=1
    else
        AC_MSG_ERROR([$CCACHE is not executable])
    fi
fi

AC_SUBST(MOZ_USING_CCACHE)
])
