MCS ?= mcs
MCSFLAGS ?= /debug

all: tests

test.h: test.idl
	/usr/lib/mozilla-1.6/xpidl -m header -I /usr/share/idl/mozilla-1.6 test.idl

test.xpt: test.idl
	/usr/lib/mozilla-1.6/xpidl -m typelib -I /usr/share/idl/mozilla-1.6 test.idl

test.so: test.h test.cpp
	c++ -g -shared -o test.so `pkg-config --cflags mozilla-xpcom` test.cpp `pkg-config --libs mozilla-xpcom`

%.exe: %.cs
	$(MCS) $(MCSFLAGS) -unsafe /out:$@ /r:../Mozilla.XPCOM.Interfaces.dll /r:../xpcom-dotnet.dll $<

TESTOBJS=test-invoke.exe test.so test.xpt generate-assembly.exe sample-app.exe loader.exe

tests: $(TESTOBJS)

clean:
	rm -f test.h $(TESTOBJS)
