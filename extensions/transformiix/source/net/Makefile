target: make_netlib

CC = g++

#ifndef PROJ_PATH
BASE_PATH     = ../base
#endif

INCLUDE_PATHS = -I . -I$(BASE_PATH) -I-

BASE_OBJS = $(BASE_PATH)/String.o

NET_OBJS = URIUtils.o

ALL_OBJS = $(BASE_OBJS) \
           $(NET_OBJS)


make_netlib: $(ALL_OBJS)

URIUtils.o: URIUtils.h URIUtils.cpp
	$(CC) $(INCLUDE_PATHS) -c URIUtils.cpp
