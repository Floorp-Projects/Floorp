target: clean

PROJ_PATH         = ${PWD}
ROOT_PATH         = $(PROJ_PATH)
XML_PATH          = $(ROOT_PATH)/xml
XMLUTIL_PATH      = $(XML_PATH)/util
XSLT_PATH         = $(ROOT_PATH)/xslt
BASE_PATH         = $(ROOT_PATH)/base
DOM_PATH          = $(XML_PATH)/dom
NET_PATH          = $(ROOT_PATH)/net
XPATH_PATH        = $(ROOT_PATH)/xpath
XSLTUTIL_PATH     = $(XSLT_PATH)/util
XMLPRINTER_PATH   = $(XML_PATH)/printer
XMLPARSER_PATH    = $(XML_PATH)/parser
EXPAT_PARSER_PATH = $(XMLPARSER_PATH)/xmlparse
EXPAT_TOKEN_PATH  = $(XMLPARSER_PATH)/xmltok

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
	cd $(XPATH_PATH);        $(CMDS)
