
INCLUDE_PATH = -I../../base -I../dom


ALL_OBJS = DOMHelper.o


target: $(ALL_OBJS)

DOMHelper.o: DOMHelper.h DOMHelper.cpp
	$(CC) $(INCLUDE_PATH) -c DOMHelper.cpp
