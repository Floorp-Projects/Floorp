target: xml_parser


BASE_PATH    = ../../base
DOM_PATH     = ../dom
PARSER_PATH  = .
NET_PATH     = ../../net

EXPAT_PARSER_PATH = xmlparse
EXPAT_TOKEN_PATH = xmltok

INCLUDE_PATH = -I$(PARSER_PATH)      \
               -I$(BASE_PATH)        \
               -I$(DOM_PATH)         \
               -I$(NET_PATH)         \
               -I$(EXPAT_TOKEN_PATH) \
               -I$(EXPAT_PARSER_PATH) -I-

EXPAT_OBJS = $(EXPAT_TOKEN_PATH)/xmltok.o \
             $(EXPAT_TOKEN_PATH)/xmlrole.o \
             $(EXPAT_PARSER_PATH)/xmlparse.o \
             $(EXPAT_PARSER_PATH)/hashtable.o

PARSER_OBJS = $(PARSER_PATH)/XMLParser.o

ALL_OBJS = $(BASE_OBJS) $(DOM_OBJS) $(EXPAT_OBJS) $(PARSER_OBJS)


CC := $(CC) -D XML_UNICODE -D __cplusplus

xml_parser: $(ALL_OBJS)

XMLParser.o: XMLParser.h XMLParser.cpp
	$(CC) $(INCLUDE_PATH) -c XMLParser.cpp

$(EXPAT_OBJS):
	make -f expat.mk
