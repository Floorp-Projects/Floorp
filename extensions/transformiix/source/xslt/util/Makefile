
ALL_OBJS = NodeStack.o


INCLUDE_PATH = -I../../base -I../..expr -I../../xml -I../../xml/dom

target: $(ALL_OBJS)

NodeStack.o: NodeStack.h NodeStack.cpp
	$(CC) $(INCLUDE_PATH) -c NodeStack.cpp
