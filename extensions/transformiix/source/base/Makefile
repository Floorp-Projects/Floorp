target: make_base

CC := $(CC) -g

BASE_OBJS = ArrayList.o               \
            CommandLineUtils.o        \
			DefaultStringComparator.o \
			Double.o \
            HashTable.o \
            Integer.o \
            List.o \
            Map.o \
            MITREObjectWrapper.o \
            NamedMap.o \
            SimpleErrorObserver.o \
            Stack.o \
            String.o \
            StringComparator.o \
            StringList.o \
            Tokenizer.o

make_base: $(BASE_OBJS)

ArrayList.o: ArrayList.h ArrayList.cpp
	$(CC) -c ArrayList.cpp

CommandLineUtils.o: CommandLineUtils.h CommandLineUtils.cpp
	$(CC) -c CommandLineUtils.cpp 

DefaultStringComparator.o: StringComparator.h DefaultStringComparator.cpp
	$(CC) -c DefaultStringComparator.cpp

Double.o: primitives.h Double.cpp
	$(CC) -c Double.cpp

HashTable.o: HashTable.h HashTable.cpp
	$(CC) -c HashTable.cpp

Integer.o: primitives.h Integer.cpp
	$(CC) -c Integer.cpp

List.o: List.h List.cpp
	$(CC) -c List.cpp 

Map.o: Map.h Map.cpp
	$(CC) -c Map.cpp

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

StringComparator.o: StringComparator.h StringComparator.cpp
	$(CC) -c StringComparator.cpp

StringList.o: String.h StringList.h StringList.cpp
	$(CC) -c StringList.cpp

Tokenizer.o: Tokenizer.h Tokenizer.cpp
	$(CC) -c Tokenizer.cpp
