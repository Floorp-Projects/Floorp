

ROOT      = ../..
BASE      = $(ROOT)/base
XPATH     = $(ROOT)/xpath
XML       = $(ROOT)/xml
XML_UTIL  = $(XML)/util
DOM       = $(XML)/dom
XSLT      = $(ROOT)/xslt

ALL_OBJS = \
	GenerateIdFunctionCall.o

INCLUDE_PATH = -I.           \
			   -I$(BASE)     \
			   -I$(XPATH)    \
			   -I$(XML)      \
			   -I$(XML_UTIL) \
			   -I$(DOM)      \
			   -I$(XSLT)     \
               -I$(XSLT)/util

target: $(ALL_OBJS)

GenerateIdFunctionCall.o: XSLTFunctions.h GenerateIdFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c GenerateIdFunctionCall.cpp









