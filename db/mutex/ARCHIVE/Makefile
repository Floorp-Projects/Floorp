# @(#)Makefile	10.2 (Sleepycat) 7/5/97

#CC=	gcc2 -Wall -O2
DIR=	../build.local
CFLAGS=	-DDEBUG -DMUTEX_PID -DMUTEX_STATISTICS -I$(DIR) -I../include

run: test.o mutex.o db_os_sleep.o
	$(CC) test.o mutex.o db_os_sleep.o -o $@

clean:
	rm -f *.o mmap.file run run.core

db_os_sleep.o: ../os/db_os_sleep.c
test.o mutex.o: Makefile
