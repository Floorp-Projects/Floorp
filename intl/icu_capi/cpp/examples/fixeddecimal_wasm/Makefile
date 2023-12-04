# This file is part of ICU4X. For terms of use, please see the file
# called LICENSE at the top level of the ICU4X source tree
# (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

.DEFAULT_GOAL := build
.PHONY: build test clean serve build-host test-host
FORCE: 

ALL_HEADERS := $(wildcard ../../include/*.hpp) $(wildcard ../../../c/include/*.h)
ICU4X_NIGHTLY_TOOLCHAIN ?= "nightly-2022-12-26"

CXX?=g++
EMCC?=emcc

$(ALL_HEADERS):

crate/target/debug/libcrate.a: FORCE
	cd crate && cargo build

a.out: crate/target/debug/libcrate.a $(ALL_HEADERS) test.cpp
	$(CXX) -std=c++17 test.cpp crate/target/debug/libcrate.a -ldl -lpthread -lm -g

crate/target/wasm32-unknown-emscripten/release/libcrate.a: FORCE
	rustup toolchain install ${ICU4X_NIGHTLY_TOOLCHAIN}
	rustup component add rust-src --toolchain ${ICU4X_NIGHTLY_TOOLCHAIN}
	RUSTFLAGS="-Cpanic=abort -Copt-level=s -Clto -Cembed-bitcode" \
	cd crate && cargo +${ICU4X_NIGHTLY_TOOLCHAIN} build --release --target wasm32-unknown-emscripten -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort

web-version.html: crate/target/wasm32-unknown-emscripten/release/libcrate.a $(ALL_HEADERS) test.cpp
	$(EMCC) -std=c++17 test.cpp crate/target/wasm32-unknown-emscripten/release/libcrate.a -ldl -lpthread -lm -g  -o web-version.html --bind --emrun -sENVIRONMENT=web -sWASM=1 -sEXPORT_ES6=1 -sMODULARIZE=1

node-version.js: crate/target/wasm32-unknown-emscripten/release/libcrate.a $(ALL_HEADERS) test.cpp
	$(EMCC) -std=c++17 test.cpp crate/target/wasm32-unknown-emscripten/release/libcrate.a -ldl -lpthread -lm -g  -o node-version.js --bind -sWASM=1 -sENVIRONMENT=node -sWASM_ASYNC_COMPILATION=0 -DNOMAIN

build: web-version.html node-version.js

test: node-version.js
	exec node ./node-test.js

serve: web-version.html
	emrun web-version.html

# These make it possible to ensure that the C++ code is up to date with the bindings
# without needing to set up emsdk. This way `make test-ffi` works without emsdk.
build-host: a.out

test-host: build-host
	./a.out

clean:
	git clean -xf *
	rm -f crate/target/wasm32-unknown-emscripten/release/libcrate.a
	rm -f crate/target/debug/libcrate.a
