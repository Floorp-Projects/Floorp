target: TransforMiiX

CC = g++

ROOT_PATH         = .
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


INCLUDE_PATHS = -I $(BASE_PATH) \
                -I $(NET_PATH) \
                -I $(DOM_PATH) \
                -I $(XML_PATH) \
                -I $(EXPR_PATH) \
                -I $(XSL_PATH) \
                -I $(XSLUTIL_PATH) \
                -I $(XMLPARSER_PATH) \
                -I $(XMLPRINTER_PATH) \
                -I $(EXPAT_PARSER_PATH) -I-

BASE_OBJS  = $(BASE_PATH)/*.o

NET_OBJS   = $(NET_PATH)/*.o

DOM_OBJS   = $(DOM_PATH)/*.o

EXPR_OBJS  = $(EXPR_PATH)/*.o

XML_OBJS   = $(XML_PATH)/*.o

XMLPRINTER_OBJS  = $(XMLPRINTER_PATH)/*.o

XMLPARSER_OBJS    = $(XMLPARSER_PATH)/*.o
EXPAT_TOKEN_OBJS  = $(EXPAT_TOKEN_PATH)/*.o
EXPAT_PARSER_OBJS = $(EXPAT_PARSER_PATH)/*.o

XSLUTIL_OBJS  =  $(XSLUTIL_PATH)/*.o
XSL_OBJS      =  $(XSL_PATH)/*.o

ALL_OBJS = $(BASE_OBJS) \
           $(NET_OBJS) \
           $(DOM_OBJS) \
           $(XSLUTIL_OBJS) \
           $(EXPR_OBJS) \
           $(XML_OBJS) \
           $(XMLPRINTER_OBJS) \
           $(XMLPARSER_OBJS) \
           $(EXPAT_TOKEN_OBJS) \
           $(EXPAT_PARSER_OBJS) \
           $(XSL_OBJS)
        
MAIN_CPP = main/transformiix.cpp

TransforMiiX: $(ALL_OBJS)
	$(CC) $(INCLUDE_PATHS) $(ALL_OBJS)  $(MAIN_CPP) -o transfrmx.exe


$(BASE_OBJS): $(BASE_PATH)/*.cpp $(BASE_PATH)/*.h
	cd $(BASE_PATH); make

$(DOM_OBJS): $(DOM_PATH)/*.cpp $(DOM_PATH)/dom.h
	cd $(DOM_PATH); make


$(XML_OBJS): $(XML_PATH)/*.cpp $(XML_PATH)/*.h
	cd $(XML_PATH); make

$(EXPAT_TOKEN_OBJS):
	cd $(XMLPARSER_PATH); make -f expat.mk

$(EXPAT_PARSER_OBJS):
	cd $(XMLPARSER_PATH); make -f expat.mk

$(XMLPARSER_OBJS):
	cd $(XMLPARSER_PATH); make
        
$(XMLPRINTER_OBJS):
	cd $(XMLPRINTER_PATH); make

$(NET_OBJS):
	cd $(NET_PATH); make

$(XSLUTIL_OBJS):
	cd $(XSLUTIL_PATH); make

$(EXPR_OBJS):
	cd $(EXPR_PATH); make

$(XSL_OBJS):
	cd $(XSL_PATH); make

