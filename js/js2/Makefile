CC = gcc
CFLAGS = -O -ggdb -DDEBUG -DXP_UNIX -Wall -W -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Wmissing-prototypes \
	-Wno-non-virtual-dtor

objs = 	hash.o \
	js2.o \
	numerics.o \
	parser.o \
	utilities.o \
	world.o

libs =	-lstdc++


%.o : %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

js2: $(objs)
	$(CC) -o $@ -ggdb $^ $(libs)

clean:
	rm -f $(objs)
