target: make_base

CC = g++

BASE_OBJS = CommandLineUtils.o \
	    Double.o \
            Integer.o \
            List.o \
            MITREObjectWrapper.o \
            NamedMap.o \
            SimpleErrorObserver.o \
            Stack.o \
            String.o \
            StringList.o \
            Tokenizer.o

make_base: $(BASE_OBJS)

CommandLineUtils.o: CommandLineUtils.h CommandLineUtils.cpp
	$(CC) -c CommandLineUtils.cpp 

Double.o: primitives.h Double.cpp
	$(CC) -c Double.cpp

Integer.o: primitives.h Integer.cpp
	$(CC) -c Integer.cpp

List.o: List.h List.cpp
	$(CC) -c List.cpp 

MITREObjectWrapper.o: MITREObject.h MITREObjectWrapper.cpp
	$(CC) -c MITREObjectWrapper.cpp

NamedMap.o: String.h NamedMap.h NamedMap.cpp
	$(CC) -c NamedMap.cpp

SimpleErrorObserver.o: String.h baseutils.h ErrorObserver.h SimpleErrorObserver.cpp
	$(CC) -c SimpleErrorObserver.cpp

Stack.o: List.h Stack.h Stack.cpp
	$(CC) -c Stack.cpp

String.o: String.h String.cpp
	$(CC) -c String.cpp

StringList.o: String.h StringList.h StringList.cpp
	$(CC) -c StringList.cpp

Tokenizer.o: Tokenizer.h Tokenizer.cpp
	$(CC) -c Tokenizer.cpp
