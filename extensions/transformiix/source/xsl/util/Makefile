BASE_PATH = ../../base
EXPR_PATH = ../expr
XML_PATH  = ../../xml
DOM_PATH  = $(XML_PATH)/dom

INCLUDE_PATHS = -I . \
                -I $(BASE_PATH) \
                -I $(EXPR_PATH) \
                -I $(DOM_PATH) \
                -I $(XML_PATH) \
                -I-

ALL_OBJS = NodeSet.o \
           NodeStack.o


CC = g++ $(INCLUDE_PATHS)

target: $(ALL_OBJS)

NodeSet.o: NodeSet.h NodeSet.cpp
	$(CC) -c NodeSet.cpp

NodeStack.o: NodeStack.h NodeStack.cpp
	$(CC) -c NodeStack.cpp