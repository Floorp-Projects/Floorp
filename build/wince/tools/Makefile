

all:
	cl ppc2003arm/arm-wince-as.c
	mv arm-wince-as.exe ppc2003arm
	cl ppc2003arm/arm-wince-gcc.c
	mv arm-wince-gcc.exe ppc2003arm
	cl ppc2003arm/arm-wince-lib.c
	mv arm-wince-lib.exe ppc2003arm
	cl ppc2003arm/arm-wince-link.c
	mv arm-wince-link.exe ppc2003arm
	rm -f *.obj
	cl ppc2003em/arm-wince-as.c
	mv arm-wince-as.exe ppc2003em
	cl ppc2003em/arm-wince-gcc.c 
	mv arm-wince-gcc.exe ppc2003em
	cl ppc2003em/arm-wince-lib.c 
	mv arm-wince-lib.exe ppc2003em
	cl ppc2003em/arm-wince-link.c
	mv arm-wince-link.exe ppc2003em
	rm -f *.obj
	cl sp2003arm/arm-wince-as.c
	mv arm-wince-as.exe sp2003arm
	cl sp2003arm/arm-wince-gcc.c
	mv arm-wince-gcc.exe sp2003arm
	cl sp2003arm/arm-wince-lib.c
	mv arm-wince-lib.exe sp2003arm
	cl sp2003arm/arm-wince-link.c
	mv arm-wince-link.exe sp2003arm
	rm -f *.obj
	cl vs8ppc2003arm/arm-wince-as.c
	mv arm-wince-as.exe vs8ppc2003arm
	cl vs8ppc2003arm/arm-wince-gcc.c
	mv arm-wince-gcc.exe vs8ppc2003arm
	cl vs8ppc2003arm/arm-wince-lib.c
	mv arm-wince-lib.exe vs8ppc2003arm
	cl vs8ppc2003arm/arm-wince-link.c
	mv arm-wince-link.exe vs8ppc2003arm
	rm -f *.obj


clobber:
	rm -f ppc2003em/*.exe
	rm -f ppc2003em/*.obj
	rm -f ppc2003arm/*.exe
	rm -f ppc2003arm/*.obj
	rm -f sp2003arm/*.exe
	rm -f sp2003arm/*.obj
	rm -f vs8ppc2003arm/*.exe
	rm -f vs8ppc2003arm/*.obj
	rm -f *.obj
