dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Set the MOZ_ICU_VERSION variable to denote the current version of the
dnl ICU library, and also the MOZ_SHARED_ICU which would be true if we are
dnl linking against a shared library of ICU, either one that we build from
dnl our copy of ICU or the system provided library.

AC_DEFUN([MOZ_CONFIG_ICU], [
    icudir="$_topsrcdir/intl/icu/source"
    if test ! -d "$icudir"; then
        icudir="$_topsrcdir/../../intl/icu/source"
        if test ! -d "$icudir"; then
            AC_MSG_ERROR([Cannot find the ICU directory])
        fi
    fi

    version=`sed -n 's/^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*U_ICU_VERSION_MAJOR_NUM[[:space:]][[:space:]]*\([0-9][0-9]*\)[[:space:]]*$/\1/p' "$icudir/common/unicode/uvernum.h"`
    if test x"$version" = x; then
       AC_MSG_ERROR([cannot determine icu version number from uvernum.h header file $lineno])
    fi
    MOZ_ICU_VERSION="$version"

    if test -z "${JS_STANDALONE}" -a -n "${JS_SHARED_LIBRARY}${MOZ_NATIVE_ICU}"; then
        MOZ_SHARED_ICU=1
    fi

    AC_SUBST(MOZ_ICU_VERSION)
    AC_SUBST(MOZ_SHARED_ICU)
])
