CC = gcc
CFLAGS = -g -ggdb -DDEBUG -DXP_UNIX -Wall -W -Wpointer-arith -Wbad-function-cast \
	-Wstrict-prototypes -Wmissing-prototypes -Wno-non-virtual-dtor

objs = 	hash.o \
	icodegenerator.o \
	interpreter.o \
	js2.o \
	jstypes.o \
	numerics.o \
	parser.o \
	utilities.o \
	world.o \
	vmtypes.o

gc_path = ../../gc/boehm/

libs =	gc.a -lstdc++ -lm

%.o : %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

js2: $(objs) gc.a
	$(CC) -o $@ -ggdb $(objs) $(libs)

gc.a:
	(cd $(gc_path) ; ln -f -s Makefile.unix Makefile ; make gc.a)
	ln -f -s $(gc_path)gc.a ./gc.a

gctest: gc_allocator.o
	$(CC) -o $@ -ggdb $^ $(libs)

clean:
	rm -f $(objs)

depend:
	gcc -MM *.cpp > dependencies

include dependencies
