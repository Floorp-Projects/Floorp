
include $(top_srcdir)/common.mk

INCLUDES = -I$(JS2_DIR)
LIBS = -L$(JS2_DIR) -ljs2 -L$(FDLIBM_DIR) -lfdm

bin_PROGRAMS = dikdik

dikdik_DEPENDENCIES = $(LIBJS2) $(LIBFDLIBM)
dikdik_SOURCES = DikDik_Shell.cpp
