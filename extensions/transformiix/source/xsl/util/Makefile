
ALL_OBJS = NodeSet.o \
           NodeStack.o

INCLUDE_PATH = -I../../base -I../expr -I../../xml -I../../xml/dom

target: $(ALL_OBJS)

NodeSet.o: NodeSet.h NodeSet.cpp
	$(CC) $(INCLUDE_PATH) -c NodeSet.cpp

NodeStack.o: NodeStack.h NodeStack.cpp
	$(CC) $(INCLUDE_PATH) -c NodeStack.cpp
