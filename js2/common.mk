
BOEHM_DIR = $(top_srcdir)/../gc/boehm/
LIBBOEHM = $(BOEHM_DIR)/gc.a

JS2_DIR = $(top_srcdir)/src/
LIBJS2 = $(JS2_DIR)/libjs2.a

WFLAGS = -Wmissing-prototypes -Wstrict-prototypes -Wunused \
         -Wswitch -Wall -Wconversion

if DEBUG
CXXFLAGS = -DXP_UNIX -g -DDEBUG $(WFLAGS)
JS1x_BINDIR = Linux_All_DBG.OBJ
else
CXXFLAGS = -DXP_UNIX -O2 -Wuninitialized $(WFLAGS)
JS1x_BINDIR = Linux_All_OPT.OBJ
endif

FDLIBM_DIR = $(top_srcdir)/../js/src/fdlibm/$(JS1x_BINDIR)
LIBFDLIBM = $(FDLIBM_DIR)/libfdm.a
