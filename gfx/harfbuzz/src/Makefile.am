# Process this file with automake to produce Makefile.in

NULL =
BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

# The following warning options are useful for debugging: -Wpadded -Wcast-align
#AM_CXXFLAGS =

lib_LTLIBRARIES = libharfbuzz.la

HBCFLAGS =
HBLIBS =
HBSOURCES =  \
	hb-blob.cc \
	hb-buffer-private.hh \
	hb-buffer.cc \
	hb-common.cc \
	hb-fallback-shape-private.hh \
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
	hb-shape.cc \
	hb-tt-font.cc \
	hb-unicode-private.hh \
	hb-unicode.cc \
	hb-warning.cc \
	$(NULL)
HBHEADERS = \
	hb.h \
	hb-blob.h \
	hb-buffer.h \
	hb-common.h \
	hb-font.h \
	hb-shape.h \
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
	hb-ot-shape-complex-arabic-table.hh \
	hb-ot-shape-complex-indic.cc \
	hb-ot-shape-complex-indic-machine.hh \
	hb-ot-shape-complex-indic-table.hh \
	hb-ot-shape-complex-misc.cc \
	hb-ot-shape-complex-private.hh \
	hb-ot-shape-normalize-private.hh \
	hb-ot-shape-normalize.cc \
	hb-ot-shape-private.hh \
	$(NULL)
HBHEADERS += \
	hb-ot.h \
	hb-ot-layout.h \
	hb-ot-tag.h \
	$(NULL)
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
HBSOURCES += hb-graphite2.cc hb-graphite2-private.hh
HBHEADERS += hb-graphite2.h
endif

if HAVE_UNISCRIBE
HBCFLAGS += $(UNISCRIBE_CFLAGS)
HBLIBS   += $(UNISCRIBE_LIBS)
HBSOURCES += hb-uniscribe.cc hb-uniscribe-private.hh
HBHEADERS += hb-uniscribe.h
endif

CXXLINK = $(LINK)
libharfbuzz_la_SOURCES = $(HBSOURCES) $(HBHEADERS)
nodist_libharfbuzz_la_SOURCES = $(nodist_HBSOURCES)
libharfbuzz_la_CPPFLAGS = $(HBCFLAGS)
libharfbuzz_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(HB_LIBTOOL_VERSION_INFO) -no-undefined
libharfbuzz_la_LIBADD = $(HBLIBS)
pkginclude_HEADERS = $(HBHEADERS)
nodist_pkginclude_HEADERS = hb-version.h


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

BUILT_SOURCES += hb-ot-shape-complex-indic-machine.hh
EXTRA_DIST += hb-ot-shape-complex-indic-machine.rl
hb-ot-shape-complex-indic-machine.hh: hb-ot-shape-complex-indic-machine.rl
	$(AM_V_GEN)$(top_srcdir)/missing --run ragel -e -F1 -o "$@.tmp" "$<" && \
	mv "$@.tmp" "$@" || ( $(RM) "$@.tmp" && false )

noinst_PROGRAMS = main
bin_PROGRAMS =

main_SOURCES = main.cc
main_CPPFLAGS = $(HBCFLAGS)
main_LDADD = libharfbuzz.la $(HBLIBS)

dist_check_SCRIPTS = \
	check-c-linkage-decls.sh \
	check-header-guards.sh \
	check-internal-symbols.sh \
	check-includes.sh \
	$(NULL)

if HAVE_ICU
else
dist_check_SCRIPTS += check-libstdc++.sh
endif

TESTS = $(dist_check_SCRIPTS)
TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	MAKE="$(MAKE) $(AM_MAKEFLAGS)" \
	HBSOURCES="$(HBSOURCES)" \
	HBHEADERS="$(HBHEADERS)" \
	$(NULL)

scan:
	g-ir-scanner $(HBCFLAGS) $(HBHEADERS) -n hb --strip-prefix=hb --library libharfbuzz.la


-include $(top_srcdir)/git.mk
