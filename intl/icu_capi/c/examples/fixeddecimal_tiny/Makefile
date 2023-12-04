# This file is part of ICU4X. For terms of use, please see the file
# called LICENSE at the top level of the ICU4X source tree
# (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

# PLEASE KEEP THIS FILE IN SYNC WITH ../segmenter_tiny/Makefile

.DEFAULT_GOAL := test
.PHONY: build test
FORCE:

ALL_HEADERS := $(wildcard ../../include/*.h)
ICU4X_NIGHTLY_TOOLCHAIN ?= "nightly-2022-12-26"

$(ALL_HEADERS):

GCC := gcc
CLANG := clang-15
LLD := lld-15

baked_data/macros.rs:
	cargo run -p icu_datagen -- --locales en bn --keys all --fallback preresolved --format mod --out baked_data --overwrite

crate/target/debug/libcrate.a: FORCE
	cd crate && cargo build --features std

crate/target/x86_64-unknown-linux-gnu/debug/libcrate.a: FORCE baked_data/macros.rs
	rustup toolchain install ${ICU4X_NIGHTLY_TOOLCHAIN}
	rustup component add rust-src --toolchain ${ICU4X_NIGHTLY_TOOLCHAIN}
	cd crate && \
	RUSTFLAGS="-Clinker-plugin-lto -Clinker=$(CLANG) -Ccodegen-units=1 -Clink-arg=-flto -Cpanic=abort" ICU4X_DATA_DIR=$(shell pwd)/baked_data cargo +${ICU4X_NIGHTLY_TOOLCHAIN} build -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort --target x86_64-unknown-linux-gnu

crate/target/x86_64-unknown-linux-gnu/release/libcrate.a: FORCE baked_data/macros.rs
	rustup toolchain install ${ICU4X_NIGHTLY_TOOLCHAIN}
	rustup component add rust-src --toolchain ${ICU4X_NIGHTLY_TOOLCHAIN}
	cd crate && \
	RUSTFLAGS="-Clto -Cembed-bitcode -Clinker-plugin-lto -Clinker=$(CLANG) -Ccodegen-units=1 -Clink-arg=-flto -Cpanic=abort -Copt-level=s" ICU4X_DATA_DIR=$(shell pwd)/baked_data cargo +${ICU4X_NIGHTLY_TOOLCHAIN} build -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort --target x86_64-unknown-linux-gnu --release

# Naive target: no optimizations, full std
optim0.elf: crate/target/debug/libcrate.a $(ALL_HEADERS) test.c
	$(GCC) test.c crate/target/debug/libcrate.a -ldl -lpthread -lm -g -o optim0.elf

# optim.elf: gcc with maximum link-time code stripping (gc-sections and strip-all)
optim1.elf: crate/target/debug/libcrate.a $(ALL_HEADERS) test.c
	$(GCC) -fdata-sections -ffunction-sections test.c crate/target/debug/libcrate.a -ldl -lpthread -lm -g -o optim1.elf -Wl,--gc-sections -Wl,--strip-all

# optim2.elf: clang single-step with gc-sections
optim2.elf: crate/target/x86_64-unknown-linux-gnu/debug/libcrate.a $(ALL_HEADERS) test.c
	$(CLANG) -flto -fdata-sections -ffunction-sections test.c crate/target/x86_64-unknown-linux-gnu/debug/libcrate.a -g -o optim2.elf -Wl,--gc-sections

optim3.o: $(ALL_HEADERS) test.c
	$(CLANG) -c -flto=thin -fdata-sections -ffunction-sections --target=x86_64-unknown-linux-gnu test.c -g -o optim3.o

# optim3.elf: clang two-step with lld, debug mode
optim3.elf: optim3.o crate/target/x86_64-unknown-linux-gnu/debug/libcrate.a
	$(CLANG) -flto=thin -fuse-ld=$(LLD) -L . -o optim3.elf optim3.o crate/target/x86_64-unknown-linux-gnu/debug/libcrate.a -Wl,--gc-sections

optim4.o: $(ALL_HEADERS) test.c
	$(CLANG) -c -flto=thin -fdata-sections -ffunction-sections --target=x86_64-unknown-linux-gnu test.c -g -o optim4.o

# optim4.elf: clang two-step with lld, release mode with debug symbols
optim4.elf: optim4.o crate/target/x86_64-unknown-linux-gnu/release/libcrate.a
	$(CLANG) -flto=thin -fuse-ld=$(LLD) -L . -o optim4.elf optim4.o crate/target/x86_64-unknown-linux-gnu/release/libcrate.a -Wl,--gc-sections

optim5.o: $(ALL_HEADERS) test.c
	$(CLANG) -c -flto=thin -fdata-sections -ffunction-sections --target=x86_64-unknown-linux-gnu test.c -o optim5.o

# optim5.elf: clang two-step with lld, release mode stripped of debug symbols
optim5.elf: optim5.o crate/target/x86_64-unknown-linux-gnu/release/libcrate.a
	$(CLANG) -flto=thin -fuse-ld=$(LLD) -L . -o optim5.elf optim5.o crate/target/x86_64-unknown-linux-gnu/release/libcrate.a -Wl,--gc-sections -Wl,--strip-all

build: optim0.elf optim1.elf optim2.elf optim3.elf optim4.elf optim5.elf
	ls -l optim*.elf

test: build
	./optim0.elf bn
	./optim1.elf bn
	./optim2.elf bn
	./optim2.elf bn
	./optim4.elf bn
	./optim5.elf bn

clean:
	git clean -xf *
