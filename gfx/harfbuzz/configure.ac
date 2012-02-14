AC_PREREQ([2.64])
AC_INIT([harfbuzz],
        [0.7.0],
        [http://bugs.freedesktop.org/enter_bug.cgi?product=harfbuzz],
        [harfbuzz],
        [http://harfbuzz.org/])

AC_CONFIG_SRCDIR([harfbuzz.pc.in])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11.1 gnu dist-bzip2 no-dist-gzip -Wall no-define])
AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Version
m4_define(hb_version_triplet,m4_split(AC_PACKAGE_VERSION,[[.]]))
m4_define(hb_version_major,m4_argn(1,hb_version_triplet))
m4_define(hb_version_minor,m4_argn(2,hb_version_triplet))
m4_define(hb_version_micro,m4_argn(3,hb_version_triplet))
HB_VERSION_MAJOR=hb_version_major
HB_VERSION_MINOR=hb_version_minor
HB_VERSION_MICRO=hb_version_micro
HB_VERSION=AC_PACKAGE_VERSION
AC_SUBST(HB_VERSION_MAJOR)
AC_SUBST(HB_VERSION_MINOR)
AC_SUBST(HB_VERSION_MICRO)
AC_SUBST(HB_VERSION)

# Libtool version
m4_define([hb_version_int],
	  m4_eval(hb_version_major*10000 + hb_version_minor*100 + hb_version_micro))
m4_if(m4_eval(hb_version_minor % 2), [1],
      dnl for unstable releases
      [m4_define([hb_libtool_revision], 0)],
      dnl for stable releases
      [m4_define([hb_libtool_revision], hb_version_micro)])
m4_define([hb_libtool_age],
	  m4_eval(hb_version_int - hb_libtool_revision))
m4_define([hb_libtool_current],
	  m4_eval(hb_version_major + hb_libtool_age))
HB_LIBTOOL_VERSION_INFO=hb_libtool_current:hb_libtool_revision:hb_libtool_age
AC_SUBST(HB_LIBTOOL_VERSION_INFO)

dnl GTK_DOC_CHECK([1.15],[--flavour no-tmpl])

# Functions and headers
AC_CHECK_FUNCS(mprotect sysconf getpagesize mmap _setmode)
AC_CHECK_HEADERS(unistd.h sys/mman.h io.h)

# Compiler flags
AC_CANONICAL_HOST
if test "x$GCC" = "xyes"; then

	# Make symbols link locally
	LDFLAGS="$LDFLAGS -Bsymbolic-functions"

	# Make sure we don't link to libstdc++
	CXXFLAGS="$CXXFLAGS -fno-rtti -fno-exceptions"

	case "$host" in
		arm-*-*)
			# Request byte alignment on arm
			CXXFLAGS="$CXXFLAGS -mstructure-size-boundary=8"
		;;
	esac
fi

dnl ==========================================================================

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, have_glib=true, have_glib=false)
if $have_glib; then
	AC_DEFINE(HAVE_GLIB, 1, [Have glib2 library])
fi
AM_CONDITIONAL(HAVE_GLIB, $have_glib)

PKG_CHECK_MODULES(GTHREAD, gthread-2.0, have_gthread=true, have_gthread=false)
if $have_gthread; then
	AC_DEFINE(HAVE_GTHREAD, 1, [Have gthread2 library])
fi
AM_CONDITIONAL(HAVE_GTHREAD, $have_gthread)

PKG_CHECK_MODULES(GOBJECT, gobject-2.0 glib-2.0 >= 2.16, have_gobject=true, have_gobject=false)
if $have_gobject; then
	AC_DEFINE(HAVE_GOBJECT, 1, [Have gobject2 library])
	GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
	AC_SUBST(GLIB_MKENUMS)
fi
AM_CONDITIONAL(HAVE_GOBJECT, $have_gobject)

dnl ==========================================================================

PKG_CHECK_MODULES(CAIRO, cairo >= 1.8.0, have_cairo=true, have_cairo=false)
if $have_cairo; then
	AC_DEFINE(HAVE_CAIRO, 1, [Have cairo graphics library])
fi
AM_CONDITIONAL(HAVE_CAIRO, $have_cairo)

PKG_CHECK_MODULES(CAIRO_FT, cairo-ft, have_cairo_ft=true, have_cairo_ft=false)
if $have_cairo_ft; then
	AC_DEFINE(HAVE_CAIRO_FT, 1, [Have cairo-ft support in cairo graphics library])
fi
AM_CONDITIONAL(HAVE_CAIRO_FT, $have_cairo_ft)

dnl ==========================================================================

PKG_CHECK_MODULES(ICU, icu, have_icu=true, [
	have_icu=true
	AC_CHECK_HEADERS(unicode/uchar.h,, have_icu=false)
	AC_MSG_CHECKING([for libicuuc])
	LIBS_old=$LIBS
	LIBS="$LIBS -licuuc"
	AC_TRY_LINK([#include <unicode/uchar.h>],
		    [u_getIntPropertyValue (0, (UProperty)0);],
		    AC_MSG_RESULT(yes),
		    AC_MSG_RESULT(no);have_icu=false)
	LIBS=$LIBS_old
	if $have_icu; then
		ICU_CFLAGS=-D_REENTRANT
		ICU_LIBS="-licuuc"
		AC_SUBST(ICU_CFLAGS)
		AC_SUBST(ICU_LIBS)
	fi
])
if $have_icu; then
	AC_DEFINE(HAVE_ICU, 1, [Have ICU library])
fi
AM_CONDITIONAL(HAVE_ICU, $have_icu)

dnl ==========================================================================

PKG_CHECK_MODULES(GRAPHITE, graphite2, have_graphite=true, have_graphite=false)
if $have_graphite; then
    AC_DEFINE(HAVE_GRAPHITE, 1, [Have Graphite library])
fi
AM_CONDITIONAL(HAVE_GRAPHITE, $have_graphite)

dnl ==========================================================================

PKG_CHECK_MODULES(FREETYPE, freetype2 >= 2.3.8, have_freetype=true, have_freetype=false)
if $have_freetype; then
	AC_DEFINE(HAVE_FREETYPE, 1, [Have FreeType 2 library])
	_save_libs="$LIBS"
	_save_cflags="$CFLAGS"
	LIBS="$LIBS $FREETYPE_LIBS"
	CFLAGS="$CFLAGS $FREETYPE_CFLAGS"
	AC_CHECK_FUNCS(FT_Face_GetCharVariantIndex)
	LIBS="$_save_libs"
	CFLAGS="$_save_cflags"
fi
AM_CONDITIONAL(HAVE_FREETYPE, $have_freetype)

dnl ===========================================================================

have_ot=true;
if $have_ot; then
	AC_DEFINE(HAVE_OT, 1, [Have native OpenType Layout backend])
fi
AM_CONDITIONAL(HAVE_OT, $have_ot)

dnl ===========================================================================

AC_CHECK_HEADERS(usp10.h windows.h, have_uniscribe=true, have_uniscribe=false)
if $have_uniscribe; then
	UNISCRIBE_CFLAGS=
	UNISCRIBE_LIBS="-lusp10 -lgdi32"
	AC_SUBST(UNISCRIBE_CFLAGS)
	AC_SUBST(UNISCRIBE_LIBS)
	AC_DEFINE(HAVE_UNISCRIBE, 1, [Have Uniscribe backend])
fi
AM_CONDITIONAL(HAVE_UNISCRIBE, $have_uniscribe)

AC_CONFIG_FILES([
Makefile
harfbuzz.pc
src/Makefile
src/hb-version.h
util/Makefile
test/Makefile
])

AC_OUTPUT
