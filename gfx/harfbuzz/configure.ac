AC_PREREQ(2.59)
AC_INIT(harfbuzz, 0.2, [http://bugs.freedesktop.org/enter_bug.cgi?product=harfbuzz])
AC_CONFIG_SRCDIR([harfbuzz.pc.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.9.6 gnu dist-bzip2 no-dist-gzip -Wall no-define])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL dnl ([1.4]) Don't remove!

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX

AC_CHECK_FUNCS(mprotect sysconf getpagesize)
AC_CHECK_HEADERS(unistd.h sys/mman.h)

# Make sure we don't link to libstdc++
if test "x$GCC" = "xyes"; then
	CXXFLAGS="$CXXFLAGS -fno-rtti -fno-exceptions"
fi

dnl ==========================================================================

PKG_CHECK_MODULES(GLIB, glib-2.0, have_glib=true, have_glib=false)
if $have_glib; then
	AC_DEFINE(HAVE_GLIB, 1, [Have glib2 library])
fi
AM_CONDITIONAL(HAVE_GLIB, $have_glib)

PKG_CHECK_MODULES(ICU, icu, have_icu=true, have_icu=false)
if $have_icu; then
	AC_DEFINE(HAVE_ICU, 1, [Have ICU library])
fi
AM_CONDITIONAL(HAVE_ICU, $have_icu)

PKG_CHECK_MODULES(GRAPHITE, silgraphite, have_graphite=true, have_graphite=false)
if $have_graphite; then
    AC_DEFINE(HAVE_GRAPHITE, 1, [Have Graphite library])
fi
AM_CONDITIONAL(HAVE_GRAPHITE, $have_graphite)

PKG_CHECK_MODULES(FREETYPE, freetype2, have_freetype=true, have_freetype=false)
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

dnl ==========================================================================

AC_CONFIG_FILES([
harfbuzz.pc
Makefile
src/Makefile
])

AC_OUTPUT
