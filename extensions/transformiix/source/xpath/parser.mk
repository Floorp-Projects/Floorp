
CC := g++ -g

ROOT_PATH    = ..
BASE_PATH    = $(ROOT_PATH)/base
XML_PATH     = $(ROOT_PATH)/xml
DOM_PATH     = $(XML_PATH)/dom/standalone
XSLT_PATH    = $(ROOT_PATH)/xslt

INCLUDE_PATHS = -I.             \
                -I$(BASE_PATH)  \
                -I$(DOM_PATH)   \
                -I$(XML_PATH)   \
                -I$(XSLT_PATH)  \
                -I-

OBJS = *.o                    \
       $(BASE_PATH)/*.o       \
       $(DOM_PATH)/*.o        \
       $(XML_PATH)/*.o        \
       $(XSLT_PATH)/Names.o




target:
	$(CC) $(INCLUDE_PATHS) $(OBJS) Parser.cpp -o parser











