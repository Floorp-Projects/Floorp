dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Set the MOZ_ICU_VERSION variable to denote the current version of the
dnl ICU library, and also the MOZ_SHARED_ICU which would be true if we are
dnl linking against a shared library of ICU, either one that we build from
dnl our copy of ICU or the system provided library.

AC_DEFUN([MOZ_CONFIG_ICU], [

ICU_LIB_NAMES=
MOZ_NATIVE_ICU=
MOZ_ARG_WITH_BOOL(system-icu,
[  --with-system-icu
                          Use system ICU (located with pkgconfig)],
    MOZ_NATIVE_ICU=1)

if test -n "$MOZ_NATIVE_ICU"; then
    PKG_CHECK_MODULES(MOZ_ICU, icu-i18n >= 50.1)
    MOZ_SHARED_ICU=1
else
    MOZ_ICU_CFLAGS='-I$(topsrcdir)/intl/icu/source/common -I$(topsrcdir)/intl/icu/source/i18n'
    AC_SUBST(MOZ_ICU_CFLAGS)
fi

AC_SUBST(MOZ_NATIVE_ICU)

MOZ_ARG_WITH_STRING(intl-api,
[  --with-intl-api, --with-intl-api=build, --without-intl-api
    Determine the status of the ECMAScript Internationalization API.  The first
    (or lack of any of these) builds and exposes the API.  The second builds it
    but doesn't use ICU or expose the API to script.  The third doesn't build
    ICU at all.],
    _INTL_API=$withval)

ENABLE_INTL_API=
EXPOSE_INTL_API=
case "$_INTL_API" in
no)
    ;;
build)
    ENABLE_INTL_API=1
    ;;
yes)
    ENABLE_INTL_API=1
    EXPOSE_INTL_API=1
    ;;
*)
    AC_MSG_ERROR([Invalid value passed to --with-intl-api: $_INTL_API])
    ;;
esac

if test -n "$EXPOSE_INTL_API"; then
    AC_DEFINE(EXPOSE_INTL_API)
fi

dnl Settings for the implementation of the ECMAScript Internationalization API
if test -n "$ENABLE_INTL_API"; then
    AC_DEFINE(ENABLE_INTL_API)

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

    if test -z "$MOZ_NATIVE_ICU"; then
        case "$OS_TARGET" in
            WINNT)
                ICU_LIB_NAMES="icuin icuuc icudt"
                if test -n "$MOZ_SHARED_ICU"; then
                    DBG_SUFFIX=
                    if test -n "$MOZ_DEBUG"; then
                        DBG_SUFFIX=d
                    fi
                    MOZ_ICU_LIBS='$(foreach lib,$(ICU_LIB_NAMES),$(DEPTH)$1/intl/icu/target/lib/$(LIB_PREFIX)$(lib)$(DBG_SUFFIX).$(LIB_SUFFIX))'
                fi
                ;;
            Darwin)
                ICU_LIB_NAMES="icui18n icuuc icudata"
                if test -n "$MOZ_SHARED_ICU"; then
                   MOZ_ICU_LIBS='$(foreach lib,$(ICU_LIB_NAMES),$(DEPTH)$1/intl/icu/target/lib/$(DLL_PREFIX)$(lib).$(MOZ_ICU_VERSION)$(DLL_SUFFIX))'
                fi
                ;;
            Linux|DragonFly|FreeBSD|NetBSD|OpenBSD)
                ICU_LIB_NAMES="icui18n icuuc icudata"
                if test -n "$MOZ_SHARED_ICU"; then
                   MOZ_ICU_LIBS='$(foreach lib,$(ICU_LIB_NAMES),$(DEPTH)$1/intl/icu/target/lib/$(DLL_PREFIX)$(lib)$(DLL_SUFFIX).$(MOZ_ICU_VERSION))'
                fi
                ;;
            *)
                AC_MSG_ERROR([ECMAScript Internationalization API is not yet supported on this platform])
        esac
        if test -z "$MOZ_SHARED_ICU"; then
            MOZ_ICU_LIBS='$(call EXPAND_LIBNAME_PATH,$(ICU_LIB_NAMES),$(DEPTH)$1/intl/icu/target/lib)'
        fi
    fi
fi

AC_SUBST(DBG_SUFFIX)
AC_SUBST(ENABLE_INTL_API)
AC_SUBST(ICU_LIB_NAMES)
AC_SUBST(MOZ_ICU_LIBS)

if test -n "$ENABLE_INTL_API" -a -z "$MOZ_NATIVE_ICU"; then
    dnl We build ICU as a static library for non-shared js builds and as a shared library for shared js builds.
    if test -z "$MOZ_SHARED_ICU"; then
        AC_DEFINE(U_STATIC_IMPLEMENTATION)
    else
        AC_DEFINE(U_COMBINED_IMPLEMENTATION)
    fi
    dnl Source files that use ICU should have control over which parts of the ICU
    dnl namespace they want to use.
    AC_DEFINE(U_USING_ICU_NAMESPACE,0)
fi


])
