target: xslt

CC := $(CC) -g


ROOT_PATH         = ..
XML_PATH          = $(ROOT_PATH)/xml
XMLUTIL_PATH      = $(XML_PATH)/util
BASE_PATH         = $(ROOT_PATH)/base
DOM_PATH          = $(XML_PATH)/dom
NET_PATH          = $(ROOT_PATH)/net
XPATH_PATH        = $(ROOT_PATH)/xpath
XSLTUTIL_PATH     = ./util
XSLTFUNC_PATH     = ./functions
XMLPRINTER_PATH   = $(XML_PATH)/printer
XMLPARSER_PATH    = $(XML_PATH)/parser
EXPAT_PARSER_PATH = $(XMLPARSER_PATH)/xmlparse
EXPAT_TOKEN_PATH  = $(XMLPARSER_PATH)/xmltok


INCLUDE_PATHS = -I.                   \
		        -I$(BASE_PATH)        \
                -I$(NET_PATH)         \
                -I$(DOM_PATH)         \
                -I$(XPATH_PATH)       \
                -I$(XSLTUTIL_PATH)    \
                -I$(XSLTFUNC_PATH)    \
                -I$(XML_PATH)         \
                -I$(XMLUTIL_PATH)     \
                -I$(XMLPARSER_PATH)   \
                -I$(XMLPRINTER_PATH)  \
                -I$(EXPAT_PARSER_PATH) -I-

XSLT_OBJS = Names.o \
            Numbering.o \
            OutputFormat.o \
            ProcessorState.o \
            VariableBinding.o \
            XSLTProcessor.o

######################
# Build Local Classes
######################

xslt: $(XSLT_OBJS)

Names.o: Names.cpp Names.h
	$(CC) -I . -I $(BASE_PATH) -c Names.cpp

Numbering.o: Numbering.cpp Numbering.h
	$(CC) $(INCLUDE_PATHS) -c Numbering.cpp

OutputFormat.o: OutputFormat.cpp OutputFormat.h
	$(CC) $(INCLUDE_PATHS) -c OutputFormat.cpp

ProcessorState.o: ProcessorState.cpp ProcessorState.h
	$(CC) $(INCLUDE_PATHS) -c ProcessorState.cpp

VariableBinding.o: VariableBinding.h VariableBinding.cpp
	$(CC) $(INCLUDE_PATHS) -c VariableBinding.cpp

XSLTProcessor.o: XSLTProcessor.cpp XSLTProcessor.h
	$(CC) -c XSLTProcessor.cpp -D__cplusplus $(INCLUDE_PATHS)
