

ROOT      = ../..
BASE      = $(ROOT)/base
NET	    = $(ROOT)/net
XPATH     = $(ROOT)/xpath
XML       = $(ROOT)/xml
XML_UTIL  = $(XML)/util
DOM       = $(XML)/dom
XSLT      = $(ROOT)/xslt
XMLPARSER_PATH    = $(XML)/parser
EXPAT_PARSER_PATH = $(XMLPARSER_PATH)/xmlparse

ALL_OBJS = \
    CurrentFunctionCall.o          \
    DocumentFunctionCall.o         \
    ElementAvailableFnCall.o       \
    FunctionAvailableFnCall.o      \
    GenerateIdFunctionCall.o       \
    SystemPropertyFunctionCall.o

INCLUDE_PATH = -I.           \
   -I$(BASE)       \
   -I$(NET)        \
   -I$(XPATH)      \
   -I$(XML)        \
   -I$(XML)/parser \
   -I$(XML_UTIL)   \
   -I$(DOM)        \
   -I$(XSLT)       \
   -I$(XSLT)/util  \
   -I$(EXPAT_PARSER_PATH) 

target: $(ALL_OBJS)

CurrentFunctionCall.o: XSLTFunctions.h CurrentFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c CurrentFunctionCall.cpp

DocumentFunctionCall.o: XSLTFunctions.h DocumentFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c DocumentFunctionCall.cpp

ElementAvailableFnCall.o: XSLTFunctions.h ElementAvailableFnCall.cpp
	$(CC) $(INCLUDE_PATH) -c ElementAvailableFnCall.cpp

FunctionAvailableFnCall.o: XSLTFunctions.h FunctionAvailableFnCall.cpp
	$(CC) $(INCLUDE_PATH) -c FunctionAvailableFnCall.cpp

GenerateIdFunctionCall.o: XSLTFunctions.h GenerateIdFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c GenerateIdFunctionCall.cpp

SystemPropertyFunctionCall.o: XSLTFunctions.h SystemPropertyFunctionCall.cpp
	$(CC) $(INCLUDE_PATH) -c SystemPropertyFunctionCall.cpp









