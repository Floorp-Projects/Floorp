CC = gcc
CFLAGS = -g -ggdb -DDEBUG -DXP_UNIX -Wall -W -Wpointer-arith -Wbad-function-cast \
	-Wstrict-prototypes -Wmissing-prototypes -Wno-non-virtual-dtor

objs = 	hash.o \
	icodegenerator.o \
	interpreter.o \
	js2.o \
	numerics.o \
	parser.o \
	utilities.o \
	world.o \
	vmtypes.o

libs =	../../gc/boehm/gc.a -lstdc++ -lm

%.o : %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

js2: $(objs)
	$(CC) -o $@ -ggdb $^ $(libs)

gctest: gc_allocator.o
	$(CC) -o $@ -ggdb $^ $(libs)

clean:
	rm -f $(objs)
