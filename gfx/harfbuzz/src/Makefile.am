# Process this file with automake to produce Makefile.in

NULL =
EXTRA_DIST = 

# The following warning options are useful for debugging: -Wpadded -Wcast-align
#AM_CXXFLAGS =

lib_LTLIBRARIES = libharfbuzz.la

HBCFLAGS =
HBLIBS =
HBSOURCES =  \
	hb-blob.c \
	hb-blob-private.h \
	hb-buffer.cc \
	hb-buffer-private.hh \
	hb-common.c \
	hb-font.cc \
	hb-font-private.h \
	hb-object-private.h \
	hb-open-file-private.hh \
	hb-open-type-private.hh \
	hb-language.c \
	hb-ot-head-private.hh \
	hb-private.h \
	hb-shape.cc \
	hb-unicode.c \
	hb-unicode-private.h \
	$(NULL)
HBHEADERS = \
	hb.h \
	hb-blob.h \
	hb-buffer.h \
	hb-common.h \
	hb-font.h \
	hb-language.h \
	hb-shape.h \
	hb-unicode.h \
	$(NULL)

HBSOURCES += \
	hb-ot-layout.cc \
	hb-ot-layout-common-private.hh \
	hb-ot-layout-gdef-private.hh \
	hb-ot-layout-gpos-private.hh \
	hb-ot-layout-gsubgpos-private.hh \
	hb-ot-layout-gsub-private.hh \
	hb-ot-layout-private.hh \
	hb-ot-map.cc \
	hb-ot-map-private.hh \
	hb-ot-shape.cc \
	hb-ot-shape-complex-arabic.cc \
	hb-ot-shape-complex-arabic-table.h \
	hb-ot-shape-complex-private.hh \
	hb-ot-shape-private.hh \
	hb-ot-tag.c \
	$(NULL)
HBHEADERS += \
	hb-ot.h \
	hb-ot-layout.h \
	hb-ot-shape.h \
	hb-ot-tag.h \
	$(NULL)

if HAVE_GLIB
HBCFLAGS += $(GLIB_CFLAGS)
HBLIBS   += $(GLIB_LIBS)
HBSOURCES += \
	hb-glib.c \
	$(NULL)
HBHEADERS += \
	hb-glib.h \
	$(NULL)
endif

if HAVE_ICU
HBCFLAGS += $(ICU_CFLAGS)
HBLIBS   += $(ICU_LIBS)
HBSOURCES += \
	hb-icu.c \
	$(NULL)
HBHEADERS += \
	hb-icu.h \
	$(NULL)
endif

if HAVE_FREETYPE
HBCFLAGS += $(FREETYPE_CFLAGS)
HBLIBS   += $(FREETYPE_LIBS)
HBSOURCES += \
	hb-ft.c \
	$(NULL)
HBHEADERS += \
	hb-ft.h \
	$(NULL)
endif

if HAVE_GRAPHITE
HBCFLAGS += $(GRAPHITE_CFLAGS)
HBLIBS   += $(GRAPHITE_LIBS)
HBSOURCES += \
	hb-graphite.cc \
	$(NULL)
HBHEADERS += \
	hb-graphite.h \
	$(NULL)
endif

CXXLINK = $(LINK)
libharfbuzz_la_SOURCES = $(HBSOURCES) $(HBHEADERS)
libharfbuzz_la_CPPFLAGS = $(HBCFLAGS)
libharfbuzz_la_LIBADD = $(HBLIBS)
pkginclude_HEADERS = $(HBHEADERS)


GENERATORS = \
	gen-arabic-joining-table.py \
	$(NULL)

EXTRA_DIST += $(GENERATORS)

noinst_PROGRAMS = main test

main_SOURCES = main.cc
main_CPPFLAGS = $(HBCFLAGS)
main_LDADD = libharfbuzz.la $(HBLIBS)

test_SOURCES = test.c
test_CPPFLAGS = $(HBCFLAGS)
test_LDADD = libharfbuzz.la $(HBLIBS)

TESTS = \
	check-c-linkage-decls.sh \
	check-header-guards.sh \
	check-internal-symbols.sh \
	$(NULL)

if HAVE_ICU
else
if HAVE_GRAPHITE
else
TESTS += check-libstdc++.sh
endif
endif

-include $(top_srcdir)/git.mk
