
CC = g++

ROOT_PATH    = ../..
BASE_PATH    = $(ROOT_PATH)/base
XML_PATH     = $(ROOT_PATH)/xml
DOM_PATH     = $(XML_PATH)/dom
XSL_PATH     = $(ROOT_PATH)/xsl
XSLUTIL_PATH = $(XSL_PATH)/util

INCLUDE_PATHS = -I . \
                -I $(BASE_PATH) \
                -I $(XML_PATH) \
                -I $(DOM_PATH) \
                -I $(XSL_PATH) \
                -I $(XSLUTIL_PATH) \
                -I-

OBJS = *.o \
       $(BASE_PATH)/*.o \
       $(XML_PATH)/*.o \
       $(DOM_PATH)/*.o \
       $(XSL_PATH)/Names.o \
       $(XSLUTIL_PATH)/*.o

target:
	$(CC) $(INCLUDE_PATHS) $(OBJS) parser.cpp -o parser.exe

