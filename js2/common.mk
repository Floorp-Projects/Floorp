
BOEHM_DIR = $(top_srcdir)/../gc/boehm/
LIBBOEHM = $(BOEHM_DIR)/gc.a

JS2_DIR = $(top_srcdir)/src/
LIBJS2 = $(JS2_DIR)/libjs2.a

WFLAGS = -Wmissing-prototypes -Wstrict-prototypes -Wunused \
         -Wswitch


if DEBUG
CXXFLAGS = -DXP_UNIX -g -DDEBUG -DNEW_PARSER $(WFLAGS)
else
CXXFLAGS = -DXP_UNIX -O2 -DNEW_PARSER -Wuninitialized $(WFLAGS)
endif
