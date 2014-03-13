# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/. */

CC=emcc

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

%.o: %.cc
	$(CC) -Os -I. -o $@ $<

FLAGS=-s USE_TYPED_ARRAYS=2 -O3 -s INLINING_LIMIT=1 --llvm-lto 1 --memory-init-file 1 --closure 1

all: $(SOURCES:.cc=.o)
	$(CC) $(FLAGS) -I. -o cld-worker.js $^ --post-js post.js -s EXPORTED_FUNCTIONS="['_detectLangCode', '_lastResultReliable']"

clean:
	rm -rf $(SOURCES:.cc=.o)
