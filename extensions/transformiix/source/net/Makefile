target: netlib

CC := $(CC) -g

ROOT        = ..
BASE_PATH   = $(ROOT)/base

INCLUDE_PATHS = -I. -I$(BASE_PATH) -I-

ALL_OBJS = URIUtils.o

netlib: $(ALL_OBJS)

URIUtils.o: URIUtils.h URIUtils.cpp
	$(CC) $(INCLUDE_PATHS) -c URIUtils.cpp






