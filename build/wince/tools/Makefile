

all:
	cl vs8ppc2003arm/arm-wince-as.c
	mv arm-wince-as.exe vs8ppc2003arm
	cl vs8ppc2003arm/arm-wince-gcc.c
	mv arm-wince-gcc.exe vs8ppc2003arm
	cl vs8ppc2003arm/arm-wince-lib.c
	mv arm-wince-lib.exe vs8ppc2003arm
	cl vs8ppc2003arm/arm-wince-link.c
	mv arm-wince-link.exe vs8ppc2003arm
	rm -f *.obj
	devenv ../shunt/build/vs8/mozce_shunt_static.sln //rebuild "Release|Windows Mobile 6 Professional SDK (ARMV4I)"

clobber:
	rm -f vs8ppc2003arm/*.exe
	rm -f vs8ppc2003arm/*.obj
	rm -f *.obj
	rm -f *.exe
	devenv ../shunt/build/vs8/mozce_shunt_static.sln /clean "Release|Windows Mobile 6 Professional SDK (ARMV4I)"

