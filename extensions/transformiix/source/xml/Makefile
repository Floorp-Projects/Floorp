
BASE_PATH = ../base
DOM_PATH  = dom

INCLUDE_PATH = -I . -I $(BASE_PATH) -I $(DOM_PATH) -I-


ALL_OBJS = XMLDOMUtils.o \
           XMLUtils.o


CC = g++ $(INCLUDE_PATH)

target: $(ALL_OBJS)

XMLDOMUtils.o: XMLDOMUtils.h XMLDOMUtils.cpp
	$(CC) -c XMLDOMUtils.cpp

XMLUtils.o: XMLUtils.h XMLUtils.cpp
	$(CC) -c XMLUtils.cpp
