CXXFLAGS = -shared -g -fPIC -fno-rtti $(shell pkg-config --cflags mozilla-xpcom)
LDFLAGS = $(shell pkg-config --libs mozilla-xpcom)

all: xpcom-dotnet.so xpcom-dotnet.dll

MCS ?= mcs
MCSFLAGS ?= /debug

xpcom-dotnet.so: typeinfo.cpp xpcom-core.cpp wrapped-clr.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

xpcom-dotnet.dll: typeinfo.cs xptinvoke.cs wrapped-clr.cs components.cs proxy-generator.cs interface-generator.cs
	$(MCS) $(MCSFLAGS) -unsafe /t:library /out:$@ $^

clean:
	rm -f xpcom-dotnet.{so,dll}
