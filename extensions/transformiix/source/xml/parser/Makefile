target: xml_parser


BASE_PATH    = ../../base
DOM_PATH     = ../dom
PARSER_PATH  = .

EXPAT_PARSER_PATH = xmlparse
EXPAT_TOKEN_PATH = xmltok

INCLUDE_PATH = -I $(PARSER_PATH) -I $(BASE_PATH) -I $(DOM_PATH) \
               -I $(EXPAT_TOKEN_PATH) -I $(EXPAT_PARSER_PATH) -I-

BASE_OBJS = $(BASE_PATH)/String.o

DOM_OBJS  = $(DOM_PATH)/NodeDefinition.o \
            $(DOM_PATH)/Document.o \
            $(DOM_PATH)/DocumentFragment.o \
            $(DOM_PATH)/NamedNodeMap.o \
            $(DOM_PATH)/NodeListDefinition.o \
            $(DOM_PATH)/Element.o \
            $(DOM_PATH)/Attr.o \
            $(DOM_PATH)/CharacterData.o \
            $(DOM_PATH)/Text.o \
            $(DOM_PATH)/Comment.o \
            $(DOM_PATH)/CDATASection.o \
            $(DOM_PATH)/ProcessingInstruction.o \
            $(DOM_PATH)/Notation.o \
            $(DOM_PATH)/Entity.o $(DOM_PATH)EntityReference.o \
            $(DOM_PATH)/DocumentType.o \
            $(DOM_PATH)/DOMImplementation.o

EXPAT_OBJS = $(EXPAT_TOKEN_PATH)/xmltok.o \
             $(EXPAT_TOKEN_PATH)/xmlrole.o \
             $(EXPAT_PARSER_PATH)/xmlparse.o \
             $(EXPAT_PARSER_PATH)/hashtable.o

PARSER_OBJS = $(PARSER_PATH)/XMLParser.o

ALL_OBJS = $(BASE_OBJS) $(DOM_OBJS) $(EXPAT_OBJS) $(PARSER_OBJS)


CC = g++ -D XML_UNICODE -D __cplusplus

xml_parser: $(ALL_OBJS)

XMLParser.o: XMLParser.h XMLParser.cpp
	$(CC) $(INCLUDE_PATH) -c XMLParser.cpp

$(EXPAT_OBJS):
	make -f expat.mk

$(DOM_OBJS):
	cd $(DOM_PATH); make

$(BASE_OBJS):
	cd $(BASE_PATH); make
