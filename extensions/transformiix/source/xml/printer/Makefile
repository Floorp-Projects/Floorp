target: printers


BASE_PATH = "../../base"
DOM_PATH  = "../dom"

INCLUDE_PATH = -I . -I $(BASE_PATH) -I $(DOM_PATH) -I-


ALL_OBJS = XMLPrinter.o \
           HTMLPrinter.o

CC = g++

printers: $(ALL_OBJS)

XMLPrinter.o: Printers.h XMLPrinter.cpp
	$(CC) $(INCLUDE_PATH) -c XMLPrinter.cpp

HTMLPrinter.o: Printers.h XMLPrinter.cpp HTMLPrinter.cpp
	$(CC) $(INCLUDE_PATH) -c HTMLPrinter.cpp
