EXPAT_PARSER_PATH = xmlparse
EXPAT_TOKEN_PATH = xmltok

EXPAT_OBJS = $(EXPAT_TOKEN_PATH)/xmltok.o \
             $(EXPAT_TOKEN_PATH)/xmlrole.o \
             $(EXPAT_PARSER_PATH)/xmlparse.o \
             $(EXPAT_PARSER_PATH)/hashtable.o


INCLUDE_PATH = -I . -I $(EXPAT_PARSER_PATH) -I $(EXPAT_TOKEN_PATH) -I-

FLAGS = -D XML_UNICODE
CC = gcc $(FLAGS) $(INCLUDE_PATH)

target: $(EXPAT_OBJS)


xmltok.o xmlrole.o: 
	cd $(EXPAT_TOKEN_PATH); \
	$(CC) -c xmltok.c xmlrole.c


xmlparse.o hashtable.o:
	cd $(EXPAT_PARSER_PATH); \
	$(CC) -c xmlparse.c hashtable.c
