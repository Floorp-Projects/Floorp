# Note: All source files are listed in Makefile.sources.

include $(top_srcdir)/build/Makefile.am.common
include $(srcdir)/Makefile.am.features

EXTRA_DIST += Makefile.win32      Makefile.win32.features
#MAINTAINERCLEANFILES += $(srcdir)/Makefile.win32.features

AM_CPPFLAGS = -I$(srcdir) $(CAIRO_CFLAGS)
AM_LDFLAGS = $(CAIRO_LDFLAGS)

if OS_WIN32
export_symbols = -export-symbols cairo.def
cairo_def_dependency = cairo.def
endif

$(top_builddir)/config.h: $(top_srcdir)/config.h.in
	cd $(top_builddir) && $(MAKE) $(AM_MAKEFLAGS) config.h

cairoincludedir = $(includedir)/cairo
cairoinclude_HEADERS = $(enabled_cairo_headers)

lib_LTLIBRARIES = libcairo.la

if BUILD_CXX
cairo_cxx_lib = libcairo_cxx.la
else
cairo_cxx_lib =
endif

noinst_LTLIBRARIES = $(cairo_cxx_lib)
libcairo_cxx_la_SOURCES = \
	$(enabled_cairo_headers) \
	$(enabled_cairo_private) \
	$(enabled_cairo_cxx_sources) \
	$(NULL)
libcairo_cxx_la_LDFLAGS = $(AM_LDFLAGS) $(export_symbols)
libcairo_cxx_la_LIBADD = $(CAIRO_LIBS)
libcairo_cxx_la_DEPENDENCIES = $(cairo_def_dependency)


libcairo_la_SOURCES = \
	$(enabled_cairo_headers) \
	$(enabled_cairo_private) \
	$(enabled_cairo_sources) \
	$(NULL)
libcairo_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(CAIRO_LIBTOOL_VERSION_INFO) -no-undefined $(export_symbols)
libcairo_la_LIBADD = $(CAIRO_LIBS) \
	$(cairo_cxx_lib)
libcairo_la_DEPENDENCIES = $(cairo_def_dependency) $(cairo_cxx_lib)

# Special headers
nodist_cairoinclude_HEADERS = cairo-features.h
nodist_libcairo_la_SOURCES  = cairo-features.h
BUILT_SOURCES  += cairo-features.h cairo-supported-features.h
DISTCLEANFILES += cairo-features.h cairo-supported-features.h
cairo-features.h cairo-supported-features.h:
	cd $(top_builddir) && ./config.status src/$@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(enabled_cairo_pkgconf)

CLEANFILES += cairo.def
cairo.def: cairo-features.h $(enabled_cairo_headers)
	@echo Generating $@
	@(echo EXPORTS; \
	(cd $(srcdir); cat $(enabled_cairo_headers) || echo 'cairo_ERROR ()' ) | \
	$(EGREP) -v '^# *include' | \
	( cat cairo-features.h - | $(CPP) -D__cplusplus - || echo 'cairo_ERROR ()' ) | \
	$(EGREP) '^cairo_.* \(' | \
	sed -e 's/[ 	].*//' | \
	sort; \
	echo LIBRARY libcairo-$(CAIRO_VERSION_SONUM).dll; \
	) >$@
	@ ! grep -q cairo_ERROR $@ || ($(RM) $@; false)

TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	MAKE="$(MAKE) $(AM_MAKEFLAGS)" \
	all_cairo_files="$(all_cairo_files)" \
	all_cairo_headers="$(all_cairo_headers)" \
	all_cairo_private="$(all_cairo_private)" \
	all_cairo_sources="$(all_cairo_sources)" \
	enabled_cairo_headers="$(enabled_cairo_headers)" \
	enabled_cairo_private="$(enabled_cairo_private)" \
	enabled_cairo_sources="$(enabled_cairo_sources)" \
	$(NULL)
TESTS_SH = \
	check-def.sh \
	check-doc-syntax.sh \
	check-headers.sh \
	check-plt.sh \
	check-preprocessor-syntax.sh \
	$(NULL)
TESTS += $(TESTS_SH)
if CROSS_COMPILING
else
TESTS += check-link$(EXEEXT)
endif

EXTRA_DIST += $(TESTS_SH) check-has-hidden-symbols.c check-doc-syntax.awk
check_PROGRAMS += check-link
check_link_LDADD = libcairo.la

check: headers-standalone

PREPROCESS_ARGS = $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS)
COMPILE_ARGS = $(PREPROCESS_ARGS) $(AM_CFLAGS) $(CFLAGS)

# The pre-processed result is used by check-{def,plt}.sh to determine whether
# cairo has been compiled with symbol hiding.
.c.i: $(cairoinclude_HEADERS) $(nodist_cairoinclude_HEADERS) cairoint.h $(top_builddir)/config.h
	$(CPP) $(PREPROCESS_ARGS) $< -o $@
.c.s: $(cairoinclude_HEADERS) $(nodist_cairoinclude_HEADERS) cairoint.h $(top_builddir)/config.h
	$(CC) $(COMPILE_ARGS) $< -S -o $@

include $(srcdir)/Makefile.am.analysis
