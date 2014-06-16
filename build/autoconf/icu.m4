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
                MOZ_ICU_DBG_SUFFIX=
                if test -n "$MOZ_DEBUG" -a -z "$MOZ_NO_DEBUG_RTL"; then
                    MOZ_ICU_DBG_SUFFIX=d
                fi
                if test -n "$MOZ_SHARED_ICU"; then
                    MOZ_ICU_LIBS='$(foreach lib,$(ICU_LIB_NAMES),$(DEPTH)/intl/icu/target/lib/$(LIB_PREFIX)$(lib)$(MOZ_ICU_DBG_SUFFIX).$(LIB_SUFFIX))'
                fi
                ;;
            Darwin)
                ICU_LIB_NAMES="icui18n icuuc icudata"
                if test -n "$MOZ_SHARED_ICU"; then
                   MOZ_ICU_LIBS='$(foreach lib,$(ICU_LIB_NAMES),$(DEPTH)/intl/icu/target/lib/$(DLL_PREFIX)$(lib).$(MOZ_ICU_VERSION)$(DLL_SUFFIX))'
                fi
                ;;
            Linux|DragonFly|FreeBSD|NetBSD|OpenBSD|GNU/kFreeBSD)
                ICU_LIB_NAMES="icui18n icuuc icudata"
                if test -n "$MOZ_SHARED_ICU"; then
                   MOZ_ICU_LIBS='$(foreach lib,$(ICU_LIB_NAMES),$(DEPTH)/intl/icu/target/lib/$(DLL_PREFIX)$(lib)$(DLL_SUFFIX).$(MOZ_ICU_VERSION))'
                fi
                ;;
            *)
                AC_MSG_ERROR([ECMAScript Internationalization API is not yet supported on this platform])
        esac
        if test -z "$MOZ_SHARED_ICU"; then
            MOZ_ICU_LIBS='$(call EXPAND_LIBNAME_PATH,$(addsuffix $(MOZ_ICU_DBG_SUFFIX),$(ICU_LIB_NAMES)),$(DEPTH)/intl/icu/target/lib)'
        fi
    fi
fi

AC_SUBST(MOZ_ICU_DBG_SUFFIX)
AC_SUBST(ENABLE_INTL_API)
AC_SUBST(ICU_LIB_NAMES)
AC_SUBST(MOZ_ICU_LIBS)

if test -n "$ENABLE_INTL_API" -a -z "$MOZ_NATIVE_ICU"; then
    dnl We build ICU as a static library for non-shared js builds and as a shared library for shared js builds.
    if test -z "$MOZ_SHARED_ICU"; then
        AC_DEFINE(U_STATIC_IMPLEMENTATION)
    fi
    dnl Source files that use ICU should have control over which parts of the ICU
    dnl namespace they want to use.
    AC_DEFINE(U_USING_ICU_NAMESPACE,0)
fi


])

AC_DEFUN([MOZ_SUBCONFIGURE_ICU], [

if test -z "$BUILDING_JS" -o -n "$JS_STANDALONE"; then

    if test -n "$ENABLE_INTL_API" -a -z "$MOZ_NATIVE_ICU"; then
        # Set ICU compile options
        ICU_CPPFLAGS=""
        # don't use icu namespace automatically in client code
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DU_USING_ICU_NAMESPACE=0"
        # don't include obsolete header files
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1"
        # remove chunks of the library that we don't need (yet)
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DUCONFIG_NO_LEGACY_CONVERSION"
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DUCONFIG_NO_TRANSLITERATION"
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DUCONFIG_NO_REGULAR_EXPRESSIONS"
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DUCONFIG_NO_BREAK_ITERATION"
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DUCONFIG_NO_IDNA"
        # we don't need to pass data to and from legacy char* APIs
        ICU_CPPFLAGS="$ICU_CPPFLAGS -DU_CHARSET_IS_UTF8"
        # make sure to not accidentally pick up system-icu headers
        ICU_CPPFLAGS="$ICU_CPPFLAGS -I$icudir/common -I$icudir/i18n"

        ICU_CROSS_BUILD_OPT=""
        ICU_SRCDIR=""
        if test "$HOST_OS_ARCH" = "WINNT"; then
    	ICU_SRCDIR="--srcdir=$(cd $srcdir/intl/icu/source; pwd -W)"
        fi

        if test "$CROSS_COMPILE"; then
    	# Building host tools.  It is necessary to build target binary.
    	case "$HOST_OS_ARCH" in
    	    Darwin)
    		ICU_TARGET=MacOSX
    		;;
    	    Linux)
    		ICU_TARGET=Linux
    		;;
    	    WINNT)
    		ICU_TARGET=MSYS/MSVC
    		;;
            DragonFly|FreeBSD|NetBSD|OpenBSD|GNU_kFreeBSD)
    		ICU_TARGET=BSD
    		;;
    	esac
    	# Remove _DEPEND_CFLAGS from HOST_FLAGS to avoid configure error
    	HOST_ICU_CFLAGS="$HOST_CFLAGS"
    	HOST_ICU_CXXFLAGS="$HOST_CXXFLAGS"

    	HOST_ICU_CFLAGS=`echo $HOST_ICU_CFLAGS | sed "s|$_DEPEND_CFLAGS||g"`
    	HOST_ICU_CXXFLAGS=`echo $HOST_ICU_CFXXLAGS | sed "s|$_DEPEND_CFLAGS||g"`

    	# ICU requires RTTI
    	if test "$GNU_CC"; then
    	    HOST_ICU_CXXFLAGS=`echo $HOST_ICU_CXXFLAGS | sed 's|-fno-rtti|-frtti|g'`
    	elif test "$_MSC_VER"; then
    	    HOST_ICU_CXXFLAGS=`echo $HOST_ICU_CXXFLAGS | sed 's|-GR-|-GR|g'`
    	fi

    	HOST_ICU_BUILD_OPTS=""
    	if test -n "$MOZ_DEBUG"; then
    	    HOST_ICU_BUILD_OPTS="$HOST_ICU_BUILD_OPTS --enable-debug"
    	fi

    	abs_srcdir=`(cd $srcdir; pwd)`
    	mkdir -p $_objdir/intl/icu/host
    	(cd $_objdir/intl/icu/host
    	 MOZ_SUBCONFIGURE_WRAP([.],[
    	 AR="$HOST_AR" RANLIB="$HOST_RANLIB" \
    	 CC="$HOST_CC" CXX="$HOST_CXX" LD="$HOST_LD" \
    	 CFLAGS="$HOST_ICU_CFLAGS $HOST_OPTIMIZE_FLAGS" \
    	 CPPFLAGS="$ICU_CPPFLAGS" \
    	 CXXFLAGS="$HOST_ICU_CXXFLAGS $HOST_OPTIMIZE_FLAGS" \
    	 LDFLAGS="$HOST_LDFLAGS" \
    		$SHELL $abs_srcdir/intl/icu/source/runConfigureICU \
    		$HOST_ICU_BUILD_OPTS \
    		$ICU_TARGET \
    	dnl Shell quoting is fun.
    		${ICU_SRCDIR+"$ICU_SRCDIR"} \
    		--enable-static --disable-shared \
    		--enable-extras=no --enable-icuio=no --enable-layout=no \
    		--enable-tests=no --enable-samples=no || exit 1
    	 ])
    	) || exit 1
    	# generate config/icucross.mk
    	$GMAKE -C $_objdir/intl/icu/host/ config/icucross.mk

    	# --with-cross-build requires absolute path
    	ICU_HOST_PATH=`cd $_objdir/intl/icu/host && pwd`
    	ICU_CROSS_BUILD_OPT="--with-cross-build=$ICU_HOST_PATH"
    	ICU_TARGET_OPT="--build=$build --host=$target"
        else
    	# CROSS_COMPILE isn't set build and target are i386 and x86-64.
    	# So we must set target for --build and --host.
    	ICU_TARGET_OPT="--build=$target --host=$target"
        fi

        if test -z "$MOZ_SHARED_ICU"; then
    	# To reduce library size, use static linking
    	ICU_LINK_OPTS="--enable-static --disable-shared"
        else
    	ICU_LINK_OPTS="--disable-static --enable-shared"
        fi
        # Force the ICU static libraries to be position independent code
        ICU_CFLAGS="$DSO_PIC_CFLAGS $CFLAGS"
        ICU_CXXFLAGS="$DSO_PIC_CFLAGS $CXXFLAGS"

        ICU_BUILD_OPTS=""
        if test -n "$MOZ_DEBUG" -o "MOZ_DEBUG_SYMBOLS"; then
    	ICU_CFLAGS="$ICU_CFLAGS $MOZ_DEBUG_FLAGS"
    	ICU_CXXFLAGS="$ICU_CXXFLAGS $MOZ_DEBUG_FLAGS"
    	if test -n "$CROSS_COMPILE" -a "$OS_TARGET" = "Darwin" \
    		-a "$HOST_OS_ARCH" != "Darwin"
    	then
    	    # Bug 951758: Cross-OSX builds with non-Darwin hosts have issues
    	    # with -g and friends (like -gdwarf and -gfull) because they try
    	    # to run dsymutil
    	    changequote(,)
    	    ICU_CFLAGS=`echo $ICU_CFLAGS | sed 's|-g[^ \t]*||g'`
    	    ICU_CXXFLAGS=`echo $ICU_CXXFLAGS | sed 's|-g[^ \t]*||g'`
    	    changequote([,])
    	fi

    	ICU_LDFLAGS="$MOZ_DEBUG_LDFLAGS"
    	if test -z "$MOZ_DEBUG"; then
    	    # To generate debug symbols, it requires MOZ_DEBUG_FLAGS.
    	    # But, not debug build.
    	    ICU_CFLAGS="$ICU_CFLAGS -UDEBUG -DNDEBUG"
    	    ICU_CXXFLAGS="$ICU_CXXFLAGS -UDEBUG -DNDEBUG"
    	elif test -z "$MOZ_NO_DEBUG_RTL"; then
    	    ICU_BUILD_OPTS="$ICU_BUILD_OPTS --enable-debug"
    	fi
        fi
        if test -z "$MOZ_OPTIMIZE"; then
    	ICU_BUILD_OPTS="$ICU_BUILD_OPTS --disable-release"
        else
    	ICU_CFLAGS="$ICU_CFLAGS $MOZ_OPTIMIZE_FLAGS"
    	ICU_CXXFLAGS="$ICU_CXXFLAGS $MOZ_OPTIMIZE_FLAGS"
        fi

        if test "$am_cv_langinfo_codeset" = "no"; then
    	# ex. Android
    	ICU_CPPFLAGS="$ICU_CPPFLAGS -DU_HAVE_NL_LANGINFO_CODESET=0"
        fi

        # ICU requires RTTI
        if test "$GNU_CC"; then
    	ICU_CXXFLAGS=`echo $ICU_CXXFLAGS | sed 's|-fno-rtti|-frtti|g'`
        else
    	if test "$_MSC_VER"; then
    	    ICU_CXXFLAGS=`echo $ICU_CXXFLAGS | sed 's|-GR-|-GR|g'`
    	fi

    	# Add RTL flags for MSVCRT.DLL
    	if test -n "$MOZ_DEBUG" -a -z "$MOZ_NO_DEBUG_RTL"; then
    	    ICU_CFLAGS="$ICU_CFLAGS -MDd"
    	    ICU_CXXFLAGS="$ICU_CXXFLAGS -MDd"
    	else
    	    ICU_CFLAGS="$ICU_CFLAGS -MD"
    	    ICU_CXXFLAGS="$ICU_CXXFLAGS -MD"
    	fi

    	# add disable optimize flag for workaround for bug 899948
    	if test -z "$MOZ_OPTIMIZE"; then
    	    ICU_CFLAGS="$ICU_CFLAGS -Od"
    	    ICU_CXXFLAGS="$ICU_CXXFLAGS -Od"
    	fi
        fi

        if test -z "$MOZ_SHARED_ICU"; then
          ICU_CXXFLAGS="$ICU_CXXFLAGS -DU_STATIC_IMPLEMENTATION"
          ICU_CFLAGS="$ICU_CFLAGS -DU_STATIC_IMPLEMENTATION"
          if test "$GNU_CC"; then
            ICU_CFLAGS="$ICU_CFLAGS -fvisibility=hidden"
            ICU_CXXFLAGS="$ICU_CXXFLAGS -fvisibility=hidden"
          fi
        fi

        # We cannot use AC_OUTPUT_SUBDIRS since ICU tree is out of spidermonkey.
        # When using AC_OUTPUT_SUBDIRS, objdir of ICU is out of objdir
        # due to relative path.
        # If building ICU moves into root of mozilla tree, we can use
        # AC_OUTPUT_SUBDIR instead.
        mkdir -p $_objdir/intl/icu/target
        (cd $_objdir/intl/icu/target
         MOZ_SUBCONFIGURE_WRAP([.],[
           AR="$AR" CC="$CC" CXX="$CXX" LD="$LD" \
           ARFLAGS="$ARFLAGS" \
           CPPFLAGS="$ICU_CPPFLAGS $CPPFLAGS" \
           CFLAGS="$ICU_CFLAGS" \
           CXXFLAGS="$ICU_CXXFLAGS" \
           LDFLAGS="$ICU_LDFLAGS $LDFLAGS" \
           $SHELL $_topsrcdir/intl/icu/source/configure \
    		$ICU_BUILD_OPTS \
    		$ICU_CROSS_BUILD_OPT \
    		$ICU_LINK_OPTS \
    		${ICU_SRCDIR+"$ICU_SRCDIR"} \
    		$ICU_TARGET_OPT \
    		--disable-extras --disable-icuio --disable-layout \
    		--disable-tests --disable-samples || exit 1
           ])
        ) || exit 1
    fi

fi

])
