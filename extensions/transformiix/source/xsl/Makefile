target: xslp

CC = g++

#ifndef PROJ_PATH
ROOT_PATH         = ..
XML_PATH          = $(ROOT_PATH)/xml
XSL_PATH          = $(ROOT_PATH)/xsl
BASE_PATH         = $(ROOT_PATH)/base
DOM_PATH          = $(XML_PATH)/dom
NET_PATH          = $(ROOT_PATH)/net
EXPR_PATH         = $(XSL_PATH)/expr
XSLUTIL_PATH      = $(XSL_PATH)/util
XMLPRINTER_PATH   = $(XML_PATH)/printer
XMLPARSER_PATH    = $(XML_PATH)/parser
EXPAT_PARSER_PATH = $(XMLPARSER_PATH)/xmlparse
EXPAT_TOKEN_PATH  = $(XMLPARSER_PATH)/xmltok
#endif

INCLUDE_PATHS = -I $(BASE_PATH) \
                -I $(NET_PATH) \
                -I $(DOM_PATH) \
                -I $(EXPR_PATH) \
                -I $(XSL_PATH) \
                -I $(XSLUTIL_PATH) \
                -I $(XML_PATH) \
                -I $(XMLPARSER_PATH) \
                -I $(XMLPRINTER_PATH) \
                -I $(EXPAT_PARSER_PATH) -I-

XSL_OBJS = Names.o \
           ProcessorState.o \
           XSLProcessor.o

######################
# Build Local Classes
######################

xslp: $(XSL_OBJS)

Names.o: Names.cpp Names.h
	$(CC) -I . -I $(BASE_PATH) -c Names.cpp

ProcessorState.o: ProcessorState.cpp ProcessorState.h
	$(CC) $(INCLUDE_PATHS) -c ProcessorState.cpp
 
XSLProcessor.o: XSLProcessor.cpp XSLProcessor.h
	$(CC) -c XSLProcessor.cpp -D __cplusplus $(INCLUDE_PATHS)
