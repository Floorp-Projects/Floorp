
INCLUDE_PATH = -I../base -Idom


ALL_OBJS = XMLDOMUtils.o \
           XMLUtils.o


target: $(ALL_OBJS)

XMLDOMUtils.o: XMLDOMUtils.h XMLDOMUtils.cpp
	$(CC) $(INCLUDE_PATH) -c XMLDOMUtils.cpp

XMLUtils.o: XMLUtils.h XMLUtils.cpp
	$(CC) $(INCLUDE_PATH) -c XMLUtils.cpp
