target: clean

CC = g++

PROJ_PATH         = ${PWD}
ROOT_PATH         = $(PROJ_PATH)
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

clean: 
	cd $(BASE_PATH); rm *.o; \
	cd $(NET_PATH); rm *.o; \
	cd $(XML_PATH); rm *.o; \
	cd $(DOM_PATH); rm *.o; \
	cd $(XMLPARSER_PATH); rm *.o; \
	cd $(EXPAT_PARSER_PATH); rm *.o; \
	cd $(EXPAT_TOKEN_PATH); rm *.o; \
	cd $(XMLPRINTER_PATH); rm *.o; \
	cd $(XSL_PATH); rm *.o; \
	cd $(XSLUTIL_PATH); rm *.o; \
	cd $(EXPR_PATH); rm *.o
