target: TransforMiiX

CC := g++

ROOT_PATH         = ${PWD}
BASE_PATH         = $(ROOT_PATH)/base
NET_PATH          = $(ROOT_PATH)/net
XML_PATH          = $(ROOT_PATH)/xml
DOM_PATH          = $(XML_PATH)/dom/standalone
XPATH_PATH        = $(ROOT_PATH)/xpath
XSLT_PATH         = $(ROOT_PATH)/xslt
XMLUTIL_PATH      = $(XML_PATH)/util
XSLTFUNC_PATH     = $(XSLT_PATH)/functions
XSLTUTIL_PATH     = $(XSLT_PATH)/util
XMLPRINTER_PATH   = $(XML_PATH)/printer
XMLPARSER_PATH    = $(XML_PATH)/parser
EXPAT_PARSER_PATH = $(XMLPARSER_PATH)/xmlparse
EXPAT_TOKEN_PATH  = $(XMLPARSER_PATH)/xmltok


INCLUDE_PATHS = -I$(BASE_PATH)          \
                -I$(NET_PATH)           \
                -I$(DOM_PATH)           \
                -I$(XML_PATH)           \
                -I$(XPATH_PATH)         \
                -I$(XSLT_PATH)          \
                -I$(XSLTFUNC_PATH)      \
		        -I$(XMLUTIL_PATH)       \
                -I$(XSLTUTIL_PATH)      \
                -I$(XMLPARSER_PATH)     \
                -I$(XMLPRINTER_PATH)    \
                -I$(EXPAT_PARSER_PATH) -I-

BASE_OBJS         = $(BASE_PATH)/*.o
NET_OBJS          = $(NET_PATH)/*.o
DOM_OBJS          = $(DOM_PATH)/*.o
XPATH_OBJS        = $(XPATH_PATH)/*.o
XML_OBJS          = $(XML_PATH)/*.o
XMLUTIL_OBJS      = $(XMLUTIL_PATH)/*.o
XMLPRINTER_OBJS   = $(XMLPRINTER_PATH)/*.o
XMLPARSER_OBJS    = $(XMLPARSER_PATH)/*.o
EXPAT_TOKEN_OBJS  = $(EXPAT_TOKEN_PATH)/*.o
EXPAT_PARSER_OBJS = $(EXPAT_PARSER_PATH)/*.o
XSLT_OBJS         = $(XSLT_PATH)/*.o
XSLTFUNC_OBJS     = $(XSLTFUNC_PATH)/*.o
XSLTUTIL_OBJS     = $(XSLTUTIL_PATH)/*.o

ALL_OBJS = $(BASE_OBJS)           \
           $(NET_OBJS)            \
           $(DOM_OBJS)            \
           $(XPATH_OBJS)          \
           $(XML_OBJS)            \
           $(XMLUTIL_OBJS)        \
           $(XMLPRINTER_OBJS)     \
           $(XMLPARSER_OBJS)      \
           $(EXPAT_TOKEN_OBJS)    \
           $(EXPAT_PARSER_OBJS)   \
           $(XSLT_OBJS)           \
           $(XSLTFUNC_OBJS)       \
	       $(XSLTUTIL_OBJS)

MAIN_CPP = main/transformiix.cpp

TransforMiiX: $(ALL_OBJS)
	$(CC) $(INCLUDE_PATHS) -I- $(ALL_OBJS)  $(MAIN_CPP) -o transfrmx


$(BASE_OBJS): $(BASE_PATH)/*.cpp $(BASE_PATH)/*.h
	cd $(BASE_PATH); make

$(DOM_OBJS): $(DOM_PATH)/*.cpp $(DOM_PATH)/dom.h
	cd $(DOM_PATH); make


$(XML_OBJS): $(XML_PATH)/*.cpp $(XML_PATH)/*.h
	cd $(XML_PATH); make

$(XMLUTIL_OBJS):
	cd $(XMLUTIL_PATH); make

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

$(XSLTUTIL_OBJS):
	cd $(XSLTUTIL_PATH); make

$(XPATH_OBJS):
	cd $(XPATH_PATH); make

$(XSLTFUNC_OBJS):
	cd $(XSLTFUNC_PATH); make

$(XSLT_OBJS):
	cd $(XSLT_PATH); make


CMDS = rm -f *.o *~; 

clean:

	cd $(BASE_PATH);         $(CMDS) \
	cd $(NET_PATH);          $(CMDS) \
	cd $(XML_PATH);          $(CMDS) \
	cd $(XMLUTIL_PATH);      $(CMDS) \
	cd $(DOM_PATH);          $(CMDS) \
	cd $(XMLPARSER_PATH);    $(CMDS) \
	cd $(EXPAT_PARSER_PATH); $(CMDS) \
	cd $(EXPAT_TOKEN_PATH);  $(CMDS) \
	cd $(XMLPRINTER_PATH);   $(CMDS) \
	cd $(XSLT_PATH);         $(CMDS) \
	cd $(XSLTUTIL_PATH);     $(CMDS) \
	cd $(XSLTFUNC_PATH);     $(CMDS) \
	cd $(XPATH_PATH);        $(CMDS)







