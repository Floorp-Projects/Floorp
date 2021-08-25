# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The entire tree should be subject to static analysis using the XPCOM
# script. Additional scripts may be added by specific subdirectories.

ifdef ENABLE_CLANG_PLUGIN
# Replace "clang-cl.exe" to "clang.exe --driver-mode=cl" to avoid loading the
# module clang.exe again when load the plugin dll, which links to the import
# library of clang.exe.
# Likewise with "clang++.exe", replacing it with "clang.exe --driver-mode=g++",
# when building .wasm files from source (we do need to keep clang++ when
# building $(WASM_ARCHIVE)). Note we'd normally use $(CPPWASMOBJS), but it's
# not defined yet when this file is included.
ifeq ($(OS_ARCH),WINNT)
CC := $(subst clang-cl.exe,clang.exe --driver-mode=cl,$(CC:.EXE=.exe))
CXX := $(subst clang-cl.exe,clang.exe --driver-mode=cl,$(CXX:.EXE=.exe))
$(notdir $(addsuffix .$(WASM_OBJ_SUFFIX),$(basename $(WASM_CPPSRCS)))): WASM_CXX := $(subst clang++.exe,clang.exe --driver-mode=g++,$(WASM_CXX:.EXE=.exe))
endif
endif
