# Process this file with automake to produce Makefile.in

NULL =
SUBDIRS =
DIST_SUBDIRS =
BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
DISTCHECK_CONFIGURE_FLAGS = --enable-introspection

# The following warning options are useful for debugging: -Wpadded
#AM_CXXFLAGS =

lib_LTLIBRARIES = libharfbuzz.la

HBCFLAGS =
HBLIBS =
HBSOURCES =  \
	hb-atomic-private.hh \
	hb-blob.cc \
	hb-buffer-private.hh \
	hb-buffer.cc \
	hb-cache-private.hh \
	hb-common.cc \
	hb-fallback-shape.cc \
	hb-font-private.hh \
	hb-font.cc \
	hb-mutex-private.hh \
	hb-object-private.hh \
	hb-open-file-private.hh \
	hb-open-type-private.hh \
	hb-ot-head-table.hh \
	hb-ot-hhea-table.hh \
	hb-ot-hmtx-table.hh \
	hb-ot-maxp-table.hh \
	hb-ot-name-table.hh \
	hb-ot-tag.cc \
	hb-private.hh \
	hb-set-private.hh \
	hb-set.cc \
	hb-shape.cc \
	hb-shape-plan-private.hh \
	hb-shape-plan.cc \
	hb-shaper-list.hh \
	hb-shaper-impl-private.hh \
	hb-shaper-private.hh \
	hb-shaper.cc \
	hb-tt-font.cc \
	hb-unicode-private.hh \
	hb-unicode.cc \
	hb-utf-private.hh \
	hb-warning.cc \
	$(NULL)
HBHEADERS = \
	hb.h \
	hb-blob.h \
	hb-buffer.h \
	hb-common.h \
	hb-font.h \
	hb-set.h \
	hb-shape.h \
	hb-shape-plan.h \
	hb-unicode.h \
	hb-version.h \
	$(NULL)

if HAVE_OT
HBSOURCES += \
	hb-ot-layout.cc \
	hb-ot-layout-common-private.hh \
	hb-ot-layout-gdef-table.hh \
	hb-ot-layout-gpos-table.hh \
	hb-ot-layout-gsubgpos-private.hh \
	hb-ot-layout-gsub-table.hh \
	hb-ot-layout-private.hh \
	hb-ot-map.cc \
	hb-ot-map-private.hh \
	hb-ot-shape.cc \
	hb-ot-shape-complex-arabic.cc \
	hb-ot-shape-complex-arabic-fallback.hh \
	hb-ot-shape-complex-arabic-table.hh \
	hb-ot-shape-complex-default.cc \
	hb-ot-shape-complex-indic.cc \
	hb-ot-shape-complex-indic-machine.hh \
	hb-ot-shape-complex-indic-private.hh \
	hb-ot-shape-complex-indic-table.hh \
	hb-ot-shape-complex-thai.cc \
	hb-ot-shape-complex-private.hh \
	hb-ot-shape-normalize-private.hh \
	hb-ot-shape-normalize.cc \
	hb-ot-shape-fallback-private.hh \
	hb-ot-shape-fallback.cc \
	hb-ot-shape-private.hh \
	$(NULL)
HBHEADERS += \
	hb-ot.h \
	hb-ot-layout.h \
	hb-ot-tag.h \
	$(NULL)
endif

if HAVE_PTHREAD
HBCFLAGS += $(PTHREAD_CFLAGS)
HBLIBS   += $(PTHREAD_LIBS)
endif

if HAVE_GLIB
HBCFLAGS += $(GLIB_CFLAGS)
HBLIBS   += $(GLIB_LIBS)
HBSOURCES += hb-glib.cc
HBHEADERS += hb-glib.h
endif

if HAVE_GOBJECT
HBCFLAGS += $(GOBJECT_CFLAGS)
HBLIBS   += $(GOBJECT_LIBS)
HBSOURCES += hb-gobject-structs.cc
nodist_HBSOURCES = hb-gobject-enums.cc
HBHEADERS += hb-gobject.h
BUILT_SOURCES += hb-gobject-enums.cc
EXTRA_DIST += hb-gobject-enums.cc.tmpl
DISTCLEANFILES += hb-gobject-enums.cc

hb-gobject-enums.cc: hb-gobject-enums.cc.tmpl $(HBHEADERS)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > "$@.tmp" && \
	mv "$@.tmp" "$@" || ( $(RM) "@.tmp" && false )
endif

if HAVE_ICU
HBCFLAGS += $(ICU_CFLAGS)
HBLIBS   += $(ICU_LIBS)
HBSOURCES += hb-icu.cc
HBHEADERS += hb-icu.h
endif

if HAVE_FREETYPE
HBCFLAGS += $(FREETYPE_CFLAGS)
HBLIBS   += $(FREETYPE_LIBS)
HBSOURCES += hb-ft.cc
HBHEADERS += hb-ft.h
endif

if HAVE_GRAPHITE2
HBCFLAGS += $(GRAPHITE2_CFLAGS)
HBLIBS   += $(GRAPHITE2_LIBS)
HBSOURCES += hb-graphite2.cc
HBHEADERS += hb-graphite2.h
endif

if HAVE_UNISCRIBE
HBCFLAGS += $(UNISCRIBE_CFLAGS)
HBLIBS   += $(UNISCRIBE_LIBS)
HBSOURCES += hb-uniscribe.cc
HBHEADERS += hb-uniscribe.h
endif

if HAVE_CORETEXT
HBCFLAGS += $(CORETEXT_CFLAGS)
HBLIBS   += $(CORETEXT_LIBS)
HBSOURCES += hb-coretext.cc
HBHEADERS += hb-coretext.h
endif

if HAVE_HB_OLD
SUBDIRS += hb-old
HBCFLAGS += -I$(srcdir)/hb-old
HBLIBS   += hb-old/libhb-old.la
HBSOURCES += hb-old.cc
endif
DIST_SUBDIRS += hb-old

if HAVE_ICU_LE
SUBDIRS += hb-icu-le
HBCFLAGS += -I$(srcdir)/hb-icu-le
HBLIBS   += hb-icu-le/libhb-icu-le.la
HBSOURCES += hb-icu-le.cc
endif
DIST_SUBDIRS += hb-icu-le

if HAVE_UCDN
SUBDIRS += hb-ucdn
HBCFLAGS += -I$(srcdir)/hb-ucdn
HBLIBS   += hb-ucdn/libhb-ucdn.la
HBSOURCES += hb-ucdn.cc
endif
DIST_SUBDIRS += hb-ucdn


# Put the library together

if OS_WIN32
export_symbols = -export-symbols harfbuzz.def
harfbuzz_def_dependency = harfbuzz.def
libharfbuzz_la_LINK = $(CXXLINK) $(libharfbuzz_la_LDFLAGS)
else
if HAVE_ICU
libharfbuzz_la_LINK = $(CXXLINK) $(libharfbuzz_la_LDFLAGS)
else
# Use a C linker, not C++; Don't link to libstdc++
libharfbuzz_la_LINK = $(LINK) $(libharfbuzz_la_LDFLAGS)
endif
endif

libharfbuzz_la_SOURCES = $(HBSOURCES) $(HBHEADERS)
nodist_libharfbuzz_la_SOURCES = $(nodist_HBSOURCES)
libharfbuzz_la_CPPFLAGS = $(HBCFLAGS)
libharfbuzz_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(HB_LIBTOOL_VERSION_INFO) $(export_symbols) -no-undefined
libharfbuzz_la_LIBADD = $(HBLIBS)
EXTRA_libharfbuzz_la_DEPENDENCIES = $(harfbuzz_def_dependency)
pkginclude_HEADERS = $(HBHEADERS)
nodist_pkginclude_HEADERS = hb-version.h

CLEANFILES += harfbuzz.def
harfbuzz.def: $(HBHEADERS)
	$(AM_V_GEN) (echo EXPORTS; \
	(cat $^ || echo 'hb_ERROR ()' ) | \
	$(EGREP) '^hb_.* \(' | \
	sed -e 's/ (.*//' | \
	LANG=C sort; \
	echo LIBRARY libharfbuzz-$(HB_VERSION_MAJOR).dll; \
	) >"$@.tmp"
	@ ! grep -q hb_ERROR "$@.tmp" && mv "$@.tmp" "$@" || ($(RM) "$@"; false)


GENERATORS = \
	gen-arabic-table.py \
	gen-indic-table.py \
	$(NULL)

EXTRA_DIST += $(GENERATORS)

unicode-tables: arabic-table indic-table

indic-table: gen-indic-table.py IndicSyllabicCategory.txt IndicMatraCategory.txt Blocks.txt
	$(AM_V_GEN) $(builddir)/$^ > hb-ot-shape-complex-indic-table.hh.tmp && \
	mv hb-ot-shape-complex-indic-table.hh.tmp $(srcdir)/hb-ot-shape-complex-indic-table.hh || \
	($(RM) hb-ot-shape-complex-indic-table.hh.tmp; false)

arabic-table: gen-arabic-table.py ArabicShaping.txt UnicodeData.txt
	$(AM_V_GEN) $(builddir)/$^ > hb-ot-shape-complex-arabic-table.hh.tmp && \
	mv hb-ot-shape-complex-arabic-table.hh.tmp $(srcdir)/hb-ot-shape-complex-arabic-table.hh || \
	($(RM) hb-ot-shape-complex-arabic-table.hh.tmp; false)


.PHONY: unicode-tables arabic-table indic-table

EXTRA_DIST += hb-ot-shape-complex-indic-machine.rl
$(srcdir)/hb-ot-shape-complex-indic-machine.hh: hb-ot-shape-complex-indic-machine.rl
	$(AM_V_GEN)$(top_srcdir)/missing --run ragel -e -F1 -o "$@.tmp" "$<" && \
	mv "$@.tmp" "$@" || ( $(RM) "$@.tmp" && false )

noinst_PROGRAMS = main test test-would-substitute test-size-params
bin_PROGRAMS =

main_SOURCES = main.cc
main_CPPFLAGS = $(HBCFLAGS)
main_LDADD = libharfbuzz.la $(HBLIBS)

test_SOURCES = test.cc
test_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

test_would_substitute_SOURCES = test-would-substitute.cc
test_would_substitute_CPPFLAGS = $(HBCFLAGS) $(FREETYPE_CFLAGS)
test_would_substitute_LDADD = libharfbuzz.la $(HBLIBS) $(FREETYPE_LIBS)

test_size_params_SOURCES = test-size-params.cc
test_size_params_CPPFLAGS = $(HBCFLAGS)
test_size_params_LDADD = libharfbuzz.la $(HBLIBS)

dist_check_SCRIPTS = \
	check-c-linkage-decls.sh \
	check-header-guards.sh \
	check-exported-symbols.sh \
	check-includes.sh \
	check-internal-symbols.sh \
	$(NULL)

if HAVE_ICU
else
dist_check_SCRIPTS += check-libstdc++.sh
endif

if HAVE_ICU_LE
else
dist_check_SCRIPTS += check-static-inits.sh
endif

TESTS = $(dist_check_SCRIPTS)
TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	MAKE="$(MAKE) $(AM_MAKEFLAGS)" \
	HBSOURCES="$(HBSOURCES)" \
	HBHEADERS="$(HBHEADERS)" \
	$(NULL)

#-include $(INTROSPECTION_MAKEFILE)
#INTROSPECTION_GIRS = hb-1.0.gir
#INTROSPECTION_SCANNER_ARGS = -I$(srcdir) -n hb --identifier-prefix=hb_
#INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
#
#if HAVE_INTROSPECTION
#
#hb-1.0.gir: libharfbuzz.la
#hb_1_0_gir_INCLUDES = GObject-2.0
#hb_1_0_gir_CFLAGS = $(INCLUDES) $(HBCFLAGS) -DHB_H -DHB_H_IN -DHB_OT_H -DHB_OT_H_IN
#hb_1_0_gir_LIBS = libharfbuzz.la
#hb_1_0_gir_FILES = $(HBHEADERS)
#
#girdir = $(datadir)/gir-1.0
#gir_DATA = $(INTROSPECTION_GIRS)
#
#typelibdir = $(libdir)/girepository-1.0
#typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)
#
#CLEANFILES += $(gir_DATA) $(typelib_DATA)
#endif

-include $(top_srcdir)/git.mk
