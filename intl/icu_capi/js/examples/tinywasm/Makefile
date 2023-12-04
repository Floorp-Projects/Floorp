# This file is part of ICU4X. For terms of use, please see the file
# called LICENSE at the top level of the ICU4X source tree
# (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

.DEFAULT_GOAL := build
FORCE:

ALL_HEADERS := $(wildcard ../../include/*)
ICU4X_NIGHTLY_TOOLCHAIN ?= "nightly-2022-12-26"

$(ALL_HEADERS):

# 100 KiB, working around a bug in older rustc
# https://github.com/unicode-org/icu4x/issues/2753
WASM_STACK_SIZE := 100000

BASEDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

lib: $(ALL_HEADERS)
	rm -rf lib;
	mkdir lib;
	cp ../../package/lib/* lib

crate/target/wasm32-unknown-unknown/release/crate.wasm: FORCE
	rustup toolchain install ${ICU4X_NIGHTLY_TOOLCHAIN}
	rustup component add rust-src --toolchain ${ICU4X_NIGHTLY_TOOLCHAIN}
	# Build the WASM library
	cd crate && \
	RUSTFLAGS="-Cpanic=abort -Copt-level=s -C link-arg=-zstack-size=${WASM_STACK_SIZE} -Clinker-plugin-lto -Ccodegen-units=1 -C linker=${BASEDIR}/ld.py -C linker-flavor=wasm-ld -Clto -Cembed-bitcode" \
	cargo +${ICU4X_NIGHTLY_TOOLCHAIN} build \
		-Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort \
		--target wasm32-unknown-unknown \
		--release

build: crate/target/wasm32-unknown-unknown/release/crate.wasm lib
	ls -l crate/target/wasm32-unknown-unknown/release/crate.wasm

test:
	node tiny.mjs

clean:
	git clean -xf *
