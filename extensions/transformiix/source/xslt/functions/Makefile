

ROOT      = ../..
BASE      = $(ROOT)/base
NET	  = $(ROOT)/net
XPATH     = $(ROOT)/xpath
XML       = $(ROOT)/xml
XML_UTIL  = $(XML)/util
DOM       = $(XML)/dom
XSLT      = $(ROOT)/xslt
XMLPARSER_PATH    = $(XML)/parser
EXPAT_PARSER_PATH = $(XMLPARSER_PATH)/xmlparse

ALL_OBJS = \
	GenerateIdFunctionCall.o \
	DocumentFunctionCall.o

INCLUDE_PATH = -I.           \
			   -I$(BASE)     \
			   -I$(NET)      \
			   -I$(XPATH)    \
			   -I$(XML)      \
			   -I$(XML)/parser      \
			   -I$(XML_UTIL) \
			   -I$(DOM)      \
			   -I$(XSLT)     \
		           -I$(XSLT)/util \
	   		   -I$(EXPAT_PARSER_PATH) 

target: $(ALL_OBJS)

GenerateIdFunctionCall.o: XSLTFunctions.h GenerateIdFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c GenerateIdFunctionCall.cpp

DocumentFunctionCall.o: XSLTFunctions.h DocumentFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c DocumentFunctionCall.cpp









