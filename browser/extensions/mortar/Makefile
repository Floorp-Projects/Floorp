# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# We keep the Makefile until the mortar is integrated into gecko build system.
ifeq ($(shell uname), Darwin)
	DLL_SUFFIX = .dylib
	CXX = g++
	CXX_FLAGS = -std=c++11 -g -fPIC
	LD_FLAGS = -dynamiclib
ifdef DEBUG
	CXX_FLAGS += -DDEBUG
endif
else ifeq ($(shell uname), Linux)
	DLL_SUFFIX = .so
	CXX = g++
	CXX_FLAGS = -std=c++11 -g -fPIC
	LD_FLAGS = -shared
ifdef DEBUG
	CXX_FLAGS += -DDEBUG
endif
else
	DLL_SUFFIX = .dll
	CXX = cl
	CXX_FLAGS = -nologo -EHsc -Oy-
ifdef DEBUG
	CXX_FLAGS += -LDd -DDEBUG -Od
else
	CXX_FLAGS += -LD
endif
	LD_FLAGS = -link -dll
endif

all : ppapi/out/rpc$(DLL_SUFFIX) ppapi/out/interpose$(DLL_SUFFIX)

ppapi/out/rpc$(DLL_SUFFIX): ppapi/out/rpc.cc host/rpc.h host/rpc.cc
	$(CXX) $(CXX_FLAGS) -I. -o $@ host/rpc.cc $(LD_FLAGS)

ppapi/out/interpose$(DLL_SUFFIX): ppapi/out/rpc.cc host/rpc.h host/interpose.cc
	$(CXX) -DINTERPOSE $(CXX_FLAGS) -I. -o $@ host/interpose.cc $(LD_FLAGS)

ppapi/out/rpc.cc: $(shell find . -name *.idl) $(shell find . -name *.py)
	cd ppapi/generators; python idl_gen_rpc.py --out ../out/rpc.cc ; cd ../..

json/test: json/json.cpp json/json.h json/test.cpp
	$(CXX) -I./json -o $@ json/test.cpp

test-json: json/test
	@./json/test && echo "OK"

clean:
	rm -rf ppapi/generators/*.pyc ppapi/generators/*~ *~ ppapi/out/* json/*~ json/test *.obj

.PHONY: all test-json test clean
