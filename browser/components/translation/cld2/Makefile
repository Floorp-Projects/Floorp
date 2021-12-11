# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

PYTHON2 ?= python2

EMSCRIPTEN_ROOT := $(shell if which emcc >/dev/null 2>&1; \
                           then dirname `which emcc`; \
                           else echo /usr/lib/emscripten; \
                           fi)

EMCC ?= $(EMSCRIPTEN_ROOT)/emcc

WEBIDL ?= $(PYTHON2) $(EMSCRIPTEN_ROOT)/tools/webidl_binder.py

# A 2MB heap is to analyze most web pages. For the outliers, we need to either
# allow for heap growth, or allocate an unreasonable amount of memory at the
# outset.
# Unfortunately, once the heap has been enlarged, there is no shrinking, so
# analyzing one 20MB web page gives us a 30-40MB heap for the life of the
# worker.
FLAGS=-s -O3 -s INLINING_LIMIT=1 -s NO_FILESYSTEM=1 -s NO_EXIT_RUNTIME=1 -s INVOKE_RUN=0 \
      -s TOTAL_STACK=8192 -s TOTAL_MEMORY=2097152 -s ALLOW_MEMORY_GROWTH=1 \
      --llvm-lto 1 --memory-init-file 1 --closure 1

export EMCC_CLOSURE_ARGS = --language_in ECMASCRIPT6 --language_out ES5_STRICT

SOURCES= \
	internal/cldutil.cc \
	internal/cldutil_shared.cc \
	internal/compact_lang_det.cc \
	internal/compact_lang_det_hint_code.cc \
	internal/compact_lang_det_impl.cc \
	internal/debug_empty.cc \
	internal/fixunicodevalue.cc \
	internal/generated_entities.cc \
	internal/generated_language.cc \
	internal/generated_ulscript.cc \
	internal/getonescriptspan.cc \
	internal/lang_script.cc \
	internal/offsetmap.cc \
	internal/scoreonescriptspan.cc \
	internal/tote.cc \
	internal/utf8statetable.cc \
	internal/cld_generated_cjk_uni_prop_80.cc \
	internal/cld2_generated_cjk_compatible.cc \
	internal/cld_generated_cjk_delta_bi_4.cc \
	internal/generated_distinct_bi_0.cc \
	internal/cld2_generated_quadchrome0122_16.cc \
	internal/cld2_generated_deltaoctachrome0122.cc \
	internal/cld2_generated_distinctoctachrome0122.cc \
	internal/cld_generated_score_quad_octa_0122_2.cc \
	cldapp.cc \
	$(NULL)

OBJECTS=$(SOURCES:.cc=.o)

default: all

%.o: %.cc Makefile
	$(EMCC) -Os -I. -o $@ $<

cldapp.o: cld.cpp

%.cpp %.js: %.idl
	$(WEBIDL) $< $*

all: cld-worker.js

cld-worker.js: $(OBJECTS) post.js cld.js
	$(EMCC) $(FLAGS) -I. -o cld-worker.js $(OBJECTS) --post-js cld.js --post-js post.js

clean:
	rm -f $(OBJECTS) cld.cpp cld.js before.js
