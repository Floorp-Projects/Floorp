

ROOT      = ../..
BASE      = $(ROOT)/base
XPATH     = $(ROOT)/xpath
XML       = $(ROOT)/xml
XML_UTIL  = $(XML)/util
DOM       = $(XML)/dom
XSLT      = $(ROOT)/xslt

ALL_OBJS = \
	NodeSorter.o \
	NodeStack.o



INCLUDE_PATH = -I.                 \
			   -I$(BASE)           \
			   -I$(XPATH)          \
			   -I$(XML)            \
			   -I$(XML_UTIL)       \
			   -I$(DOM)            \
			   -I$(XSLT)           \
               -I$(XSLT)/functions

target: $(ALL_OBJS)

NodeSorter.o: NodeSorter.h NodeSorter.cpp
	$(CC) $(INCLUDE_PATH) -c NodeSorter.cpp

NodeStack.o: NodeStack.h NodeStack.cpp
	$(CC) $(INCLUDE_PATH) -c NodeStack.cpp









