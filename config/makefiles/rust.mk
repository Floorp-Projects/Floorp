# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# /!\ In this file, we export multiple variables globally via make rather than
# in recipes via the `env` command to avoid round-trips to msys on Windows, which
# tend to break environment variable values in interesting ways.

# /!\ Avoid the use of double-quotes in this file, so that the cargo
# commands can be executed directly by make, without doing a round-trip
# through a shell.

cargo_host_flag := --target=$(RUST_HOST_TARGET)
cargo_target_flag := --target=$(RUST_TARGET)

# Permit users to pass flags to cargo from their mozconfigs (e.g. --color=always).
cargo_build_flags = $(CARGOFLAGS)
ifndef MOZ_DEBUG_RUST
cargo_build_flags += --release
endif

# The Spidermonkey library can be built from a package tarball outside the
# tree, so we want to let Cargo create lock files in this case. When built
# within a tree, the Rust dependencies have been vendored in so Cargo won't
# touch the lock file.
ifndef JS_STANDALONE
cargo_build_flags += --frozen
endif

cargo_build_flags += --manifest-path $(CARGO_FILE)
ifdef BUILD_VERBOSE_LOG
cargo_build_flags += -vv
endif

# Enable color output if original stdout was a TTY and color settings
# aren't already present. This essentially restores the default behavior
# of cargo when running via `mach`.
ifdef MACH_STDOUT_ISATTY
ifeq (,$(findstring --color,$(cargo_build_flags)))
ifeq (WINNT,$(HOST_OS_ARCH))
# Bug 1417003: color codes are non-trivial on Windows.  For now,
# prefer black and white to broken color codes.
cargo_build_flags += --color=never
else
cargo_build_flags += --color=always
endif
endif
endif

# Without -j > 1, make will not pass jobserver info down to cargo. Force
# one job when requested as a special case.
ifeq (1,$(MOZ_PARALLEL_BUILD))
cargo_build_flags += -j1
endif

# We also need to rebuild the rust stdlib so that it's instrumented. Because
# build-std is still pretty experimental, we need to explicitly request
# the panic_abort crate for `panic = "abort"` support.
ifdef MOZ_TSAN
cargo_build_flags += -Zbuild-std=std,panic_abort
RUSTFLAGS += -Zsanitizer=thread
endif

# These flags are passed via `cargo rustc` and only apply to the final rustc
# invocation (i.e., only the top-level crate, not its dependencies).
cargo_rustc_flags = $(CARGO_RUSTCFLAGS)
ifndef DEVELOPER_OPTIONS
ifndef MOZ_DEBUG_RUST
# Enable link-time optimization for release builds, but not when linking
# gkrust_gtest. And not when doing cross-language LTO.
ifndef MOZ_LTO_RUST_CROSS
ifeq (,$(findstring gkrust_gtest,$(RUST_LIBRARY_FILE)))
cargo_rustc_flags += -Clto
endif
# We need -Cembed-bitcode=yes for all crates when using -Clto.
RUSTFLAGS += -Cembed-bitcode=yes
endif
endif
endif

ifdef CARGO_INCREMENTAL
export CARGO_INCREMENTAL
endif

rustflags_neon =
ifeq (neon,$(MOZ_FPU))
# Enable neon and disable restriction to 16 FPU registers
# (CPUs with neon have 32 FPU registers available)
rustflags_neon += -C target_feature=+neon,-d16
endif

rustflags_sancov =
ifdef LIBFUZZER
ifndef MOZ_TSAN
ifndef FUZZING_JS_FUZZILLI
# These options should match what is implicitly enabled for `clang -fsanitize=fuzzer`
#   here: https://github.com/llvm/llvm-project/blob/release/8.x/clang/lib/Driver/SanitizerArgs.cpp#L354
#
#  -sanitizer-coverage-inline-8bit-counters      Increments 8-bit counter for every edge.
#  -sanitizer-coverage-level=4                   Enable coverage for all blocks, critical edges, and indirect calls.
#  -sanitizer-coverage-trace-compares            Tracing of CMP and similar instructions.
#  -sanitizer-coverage-pc-table                  Create a static PC table.
#
# In TSan builds, we must not pass any of these, because sanitizer coverage is incompatible with TSan.
rustflags_sancov += -Cpasses=sancov -Cllvm-args=-sanitizer-coverage-inline-8bit-counters -Cllvm-args=-sanitizer-coverage-level=4 -Cllvm-args=-sanitizer-coverage-trace-compares -Cllvm-args=-sanitizer-coverage-pc-table
endif
endif
endif

rustflags_override = $(MOZ_RUST_DEFAULT_FLAGS) $(rustflags_neon)

ifdef DEVELOPER_OPTIONS
# By default the Rust compiler will perform a limited kind of ThinLTO on each
# crate. For local builds this additional optimization is not worth the
# increase in compile time so we opt out of it.
rustflags_override += -Clto=off
endif

ifdef MOZ_USING_SCCACHE
export RUSTC_WRAPPER=$(CCACHE)
endif

ifdef MOZ_TSAN
ifndef CROSS_COMPILE
NATIVE_TSAN=1
endif # CROSS_COMPILE
endif # MOZ_TSAN

ifeq (WINNT,$(HOST_OS_ARCH))
ifdef MOZ_CODE_COVERAGE
WIN_CODE_COVERAGE=1
endif # MOZ_CODE_COVERAGE
endif # WINNT

# We start with host variables because the rust host and the rust target might be the same,
# in which case we want the latter to take priority.

# We're passing these for consumption by the `cc` crate, which doesn't use the same
# convention as cargo itself:
# https://github.com/alexcrichton/cc-rs/blob/baa71c0e298d9ad7ac30f0ad78f20b4b3b3a8fb2/src/lib.rs#L1715
rust_host_cc_env_name := $(subst -,_,$(RUST_HOST_TARGET))

# HOST_CC/HOST_CXX/CC/CXX usually contain base flags for e.g. the build target.
# We want to pass those through CFLAGS_*/CXXFLAGS_* instead, so that they end up
# after whatever cc-rs adds to the compiler command line, so that they win.
# Ideally, we'd use CRATE_CC_NO_DEFAULTS=1, but that causes other problems at the
# moment.
export CC_$(rust_host_cc_env_name)=$(filter-out $(HOST_CC_BASE_FLAGS),$(HOST_CC))
export CXX_$(rust_host_cc_env_name)=$(filter-out $(HOST_CXX_BASE_FLAGS),$(HOST_CXX))
# We don't have a HOST_AR. If rust needs one, assume it's going to pick an appropriate one.

rust_cc_env_name := $(subst -,_,$(RUST_TARGET))

export CC_$(rust_cc_env_name)=$(filter-out $(CC_BASE_FLAGS),$(CC))
export CXX_$(rust_cc_env_name)=$(filter-out $(CXX_BASE_FLAGS),$(CXX))
export AR_$(rust_cc_env_name)=$(AR)

ifeq (WINNT,$(HOST_OS_ARCH))
HOST_CC_BASE_FLAGS += -DUNICODE
HOST_CXX_BASE_FLAGS += -DUNICODE
CC_BASE_FLAGS += -DUNICODE
CXX_BASE_FLAGS += -DUNICODE
endif

ifeq (,$(NATIVE_TSAN)$(WIN_CODE_COVERAGE))
# -DMOZILLA_CONFIG_H is added to prevent mozilla-config.h from injecting anything
# in C/C++ compiles from rust. That's not needed in the other branch because the
# base flags don't force-include mozilla-config.h.
export CFLAGS_$(rust_host_cc_env_name)=$(HOST_CC_BASE_FLAGS) $(COMPUTED_HOST_CFLAGS) -DMOZILLA_CONFIG_H
export CXXFLAGS_$(rust_host_cc_env_name)=$(HOST_CXX_BASE_FLAGS) $(COMPUTED_HOST_CXXFLAGS) -DMOZILLA_CONFIG_H
export CFLAGS_$(rust_cc_env_name)=$(CC_BASE_FLAGS) $(COMPUTED_CFLAGS) -DMOZILLA_CONFIG_H
export CXXFLAGS_$(rust_cc_env_name)=$(CXX_BASE_FLAGS) $(COMPUTED_CXXFLAGS) -DMOZILLA_CONFIG_H
else
# Because cargo doesn't allow to distinguish builds happening for build
# scripts/procedural macros vs. those happening for the rust target,
# we can't blindly pass all our flags down for cc-rs to use them, because of the
# side effects they can have on what otherwise should be host builds.
# So for thread-sanitizer and coverage builds, we only pass the base compiler flags.
# This means C code built by rust is not going to be covered by thread-sanitizer
# and coverage. But at least we control what compiler is being used,
# rather than relying on cc-rs guesses, which, sometimes fail us.
export CFLAGS_$(rust_host_cc_env_name)=$(HOST_CC_BASE_FLAGS)
export CXXFLAGS_$(rust_host_cc_env_name)=$(HOST_CXX_BASE_FLAGS)
export CFLAGS_$(rust_cc_env_name)=$(CC_BASE_FLAGS)
export CXXFLAGS_$(rust_cc_env_name)=$(CXX_BASE_FLAGS)
endif

# Force the target down to all bindgen callers, even those that may not
# read BINDGEN_SYSTEM_FLAGS some way or another.
export BINDGEN_EXTRA_CLANG_ARGS:=$(filter --target=%,$(BINDGEN_SYSTEM_FLAGS))
export CARGO_TARGET_DIR
export RUSTFLAGS
export RUSTC
export RUSTDOC
export RUSTFMT
export MOZ_SRC=$(topsrcdir)
export MOZ_DIST=$(ABS_DIST)
export LIBCLANG_PATH=$(MOZ_LIBCLANG_PATH)
export CLANG_PATH=$(MOZ_CLANG_PATH)
export PKG_CONFIG
export PKG_CONFIG_ALLOW_CROSS=1
ifneq (,$(PKG_CONFIG_SYSROOT_DIR))
export PKG_CONFIG_SYSROOT_DIR
endif
ifneq (,$(PKG_CONFIG_LIBDIR))
export PKG_CONFIG_LIBDIR
endif
export RUST_BACKTRACE=full
export MOZ_TOPOBJDIR=$(topobjdir)
export PYTHON3
export CARGO_PROFILE_RELEASE_OPT_LEVEL
export CARGO_PROFILE_DEV_OPT_LEVEL

# Set COREAUDIO_SDK_PATH for third_party/rust/coreaudio-sys/build.rs
ifeq ($(OS_ARCH), Darwin)
ifdef MACOS_SDK_DIR
export COREAUDIO_SDK_PATH=$(MACOS_SDK_DIR)
endif
endif

ifndef RUSTC_BOOTSTRAP
ifeq (,$(filter 1.47.% 1.48.% 1.49.%,$(RUSTC_VERSION)))
RUSTC_BOOTSTRAP := gkrust_shared,qcms,xmldecl
ifdef MOZ_RUST_SIMD
RUSTC_BOOTSTRAP := $(RUSTC_BOOTSTRAP),encoding_rs,packed_simd
endif
export RUSTC_BOOTSTRAP
endif
endif

target_rust_ltoable := force-cargo-library-build
target_rust_nonltoable := force-cargo-test-run force-cargo-library-check $(foreach b,build check,force-cargo-program-$(b))

ifdef MOZ_PGO_RUST
ifdef MOZ_PROFILE_GENERATE
# Our top-level Cargo.toml sets panic to abort, so we technically don't need -C panic=abort,
# but the autocfg crate takes RUSTFLAGS verbatim and runs its compiler tests without
# -C panic=abort (because it doesn't know it's what cargo uses), which fail on Windows
# because -C panic=unwind (the compiler default) is not compatible with -C profile-generate
# (https://github.com/rust-lang/rust/issues/61002).
rust_pgo_flags := -C panic=abort -C profile-generate=$(topobjdir)
ifeq (1,$(words $(filter 5.% 6.% 7.% 8.% 9.% 10.% 11.%,$(CC_VERSION) $(RUSTC_LLVM_VERSION))))
# Disable value profiling when:
# (RUSTC_LLVM_VERSION < 12 and CC_VERSION >= 12) or (RUSTC_LLVM_VERSION >= 12 and CC_VERSION < 12)
rust_pgo_flags += -C llvm-args=--disable-vp=true
endif
# The C compiler may be passed extra llvm flags for PGO that we also want to pass to rust as well.
# In PROFILE_GEN_CFLAGS, they look like "-mllvm foo", and we want "-C llvm-args=foo", so first turn
# "-mllvm foo" into "-mllvm:foo" so that it becomes a unique argument, that we can then filter for,
# excluding other flags, and then turn into the right string.
rust_pgo_flags += $(patsubst -mllvm:%,-C llvm-args=%,$(filter -mllvm:%,$(subst -mllvm ,-mllvm:,$(PROFILE_GEN_CFLAGS))))
else # MOZ_PROFILE_USE
rust_pgo_flags := -C profile-use=$(PGO_PROFILE_PATH)
endif
endif

$(target_rust_ltoable): RUSTFLAGS:=$(rustflags_override) $(rustflags_sancov) $(RUSTFLAGS) $(if $(MOZ_LTO_RUST_CROSS),-Clinker-plugin-lto) $(rust_pgo_flags)
$(target_rust_nonltoable): RUSTFLAGS:=$(rustflags_override) $(rustflags_sancov) $(RUSTFLAGS)

TARGET_RECIPES := $(target_rust_ltoable) $(target_rust_nonltoable)

HOST_RECIPES := \
  $(foreach a,library program,$(foreach b,build check,force-cargo-host-$(a)-$(b)))

$(HOST_RECIPES): RUSTFLAGS:=$(rustflags_override)

# If this is a release build we want rustc to generate one codegen unit per
# crate. This results in better optimization and less code duplication at the
# cost of longer compile times.
ifndef DEVELOPER_OPTIONS
$(TARGET_RECIPES) $(HOST_RECIPES): RUSTFLAGS += -C codegen-units=1
endif

# We use the + prefix to pass down the jobserver fds to cargo, but we
# don't use the prefix when make -n is used, so that cargo doesn't run
# in that case)
define RUN_CARGO
$(if $(findstring n,$(filter-out --%, $(MAKEFLAGS))),,+)$(CARGO) $(1) $(cargo_build_flags)
endef

# This function is intended to be called by:
#
#   $(call CARGO_BUILD,EXTRA_ENV_VAR1=X EXTRA_ENV_VAR2=Y ...)
#
# but, given the idiosyncracies of make, can also be called without arguments:
#
#   $(call CARGO_BUILD)
define CARGO_BUILD
$(call RUN_CARGO,rustc)
endef

define CARGO_CHECK
$(call RUN_CARGO,check)
endef

cargo_host_linker_env_var := CARGO_TARGET_$(call varize,$(RUST_HOST_TARGET))_LINKER
cargo_linker_env_var := CARGO_TARGET_$(call varize,$(RUST_TARGET))_LINKER

# Cargo needs the same linker flags as the C/C++ compiler,
# but not the final libraries. Filter those out because they
# cause problems on macOS 10.7; see bug 1365993 for details.
# Also, we don't want to pass PGO flags until cargo supports them.
export MOZ_CARGO_WRAP_LDFLAGS
export MOZ_CARGO_WRAP_LD
export MOZ_CARGO_WRAP_HOST_LDFLAGS
export MOZ_CARGO_WRAP_HOST_LD
# Exporting from make always exports a value. Setting a value per-recipe
# would export an empty value for the host recipes. When not doing a
# cross-compile, the --target for those is the same, and cargo will use
# CARGO_TARGET_*_LINKER for its linker, so we always pass the
# cargo-linker wrapper, and fill MOZ_CARGO_WRAP_{HOST_,}LD* more or less
# appropriately for all recipes.
ifeq (WINNT,$(HOST_OS_ARCH))
# Use .bat wrapping on Windows hosts, and shell wrapping on other hosts.
# Like for CC/C*FLAGS, we want the target values to trump the host values when
# both variables are the same.
export $(cargo_host_linker_env_var):=$(topsrcdir)/build/cargo-host-linker.bat
export $(cargo_linker_env_var):=$(topsrcdir)/build/cargo-linker.bat
WRAP_HOST_LINKER_LIBPATHS:=$(HOST_LINKER_LIBPATHS_BAT)
else
export $(cargo_host_linker_env_var):=$(topsrcdir)/build/cargo-host-linker
export $(cargo_linker_env_var):=$(topsrcdir)/build/cargo-linker
WRAP_HOST_LINKER_LIBPATHS:=$(HOST_LINKER_LIBPATHS)
endif

# Defining all of this for TSan builds results in crashes while running
# some crates's build scripts (!), so disable it for now.
# See https://github.com/rust-lang/cargo/issues/5754
ifndef NATIVE_TSAN
$(TARGET_RECIPES): MOZ_CARGO_WRAP_LDFLAGS:=$(filter-out -fsanitize=cfi% -framework Cocoa -lobjc AudioToolbox ExceptionHandling -fprofile-%,$(LDFLAGS))
endif # NATIVE_TSAN

# When building programs with sanitizer, rustc links its own runtime, which
# conflicts with the one that passing -fsanitize=* to the linker would add.
ifneq (,$(filter -Zsanitizer=%,$(RUSTFLAGS)))
force-cargo-program-build: MOZ_CARGO_WRAP_LDFLAGS:=$(filter-out -fsanitize=%,$(MOZ_CARGO_WRAP_LDFLAGS))
endif

# Rustc assumes that *-windows-gnu targets build with mingw-gcc and manually
# add runtime libraries that don't exist with mingw-clang. We created dummy
# libraries in $(topobjdir)/build/win32, but that's not enough, because some
# of the wanted symbols that come from these libraries are available in a
# different library, that we add manually. We also need to avoid rustc
# passing -nodefaultlibs to clang so that it adds clang_rt.
ifeq (WINNT_clang,$(OS_ARCH)_$(CC_TYPE))
force-cargo-program-build: MOZ_CARGO_WRAP_LDFLAGS+=-L$(topobjdir)/build/win32 -lunwind
force-cargo-program-build: CARGO_RUSTCFLAGS += -C default-linker-libraries=yes
endif

# Rustc passes -nodefaultlibs to the linker (clang) on mac, which prevents
# clang from adding the necessary sanitizer runtimes when building with
# C/C++ sanitizer but without rust sanitizer.
ifeq (Darwin,$(OS_ARCH))
ifeq (,$(filter -Zsanitizer=%,$(RUSTFLAGS)))
ifneq (,$(filter -fsanitize=%,$(LDFLAGS)))
force-cargo-program-build: CARGO_RUSTCFLAGS += -C default-linker-libraries=yes
endif
endif
endif

$(HOST_RECIPES): MOZ_CARGO_WRAP_LDFLAGS:=$(HOST_LDFLAGS) $(WRAP_HOST_LINKER_LIBPATHS)
$(TARGET_RECIPES) $(HOST_RECIPES): MOZ_CARGO_WRAP_HOST_LDFLAGS:=$(HOST_LDFLAGS) $(WRAP_HOST_LINKER_LIBPATHS)

ifeq (,$(filter clang-cl,$(CC_TYPE)))
$(TARGET_RECIPES): MOZ_CARGO_WRAP_LD:=$(CC)
else
$(TARGET_RECIPES): MOZ_CARGO_WRAP_LD:=$(LINKER)
endif

ifeq (,$(filter clang-cl,$(HOST_CC_TYPE)))
$(HOST_RECIPES): MOZ_CARGO_WRAP_LD:=$(HOST_CC)
$(TARGET_RECIPES) $(HOST_RECIPES): MOZ_CARGO_WRAP_HOST_LD:=$(HOST_CC)
else
$(HOST_RECIPES): MOZ_CARGO_WRAP_LD:=$(HOST_LINKER)
$(TARGET_RECIPES) $(HOST_RECIPES): MOZ_CARGO_WRAP_HOST_LD:=$(HOST_LINKER)
endif

ifdef RUST_LIBRARY_FILE

ifdef RUST_LIBRARY_FEATURES
rust_features_flag := --features '$(RUST_LIBRARY_FEATURES)'
endif

# Assume any system libraries rustc links against are already in the target's LIBS.
#
# We need to run cargo unconditionally, because cargo is the only thing that
# has full visibility into how changes in Rust sources might affect the final
# build.
force-cargo-library-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) --lib $(cargo_target_flag) $(rust_features_flag) -- $(cargo_rustc_flags)

$(RUST_LIBRARY_FILE): force-cargo-library-build
# When we are building in --enable-release mode; we add an additional check to confirm
# that we are not importing any networking-related functions in rust code. This reduces
# the chance of proxy bypasses originating from rust code.
# The check only works when rust code is built with -Clto but without MOZ_LTO_RUST_CROSS.
# Sanitizers and sancov also fail because compiler-rt hooks network functions.
ifndef MOZ_PROFILE_GENERATE
ifeq ($(OS_ARCH), Linux)
ifeq (,$(rustflags_sancov)$(MOZ_ASAN)$(MOZ_TSAN)$(MOZ_UBSAN))
ifndef MOZ_LTO_RUST_CROSS
ifneq (,$(filter -Clto,$(cargo_rustc_flags)))
	$(call py_action,check_binary,--target --networking $@)
endif
endif
endif
endif
endif

force-cargo-library-check:
	$(call CARGO_CHECK) --lib $(cargo_target_flag) $(rust_features_flag)
else
force-cargo-library-check:
	@true
endif # RUST_LIBRARY_FILE

ifdef RUST_TESTS

rust_test_options := $(foreach test,$(RUST_TESTS),-p $(test))

ifdef RUST_TEST_FEATURES
rust_test_features_flag := --features '$(RUST_TEST_FEATURES)'
endif

# Don't stop at the first failure. We want to list all failures together.
rust_test_flag := --no-fail-fast

force-cargo-test-run:
	$(call RUN_CARGO,test $(cargo_target_flag) $(rust_test_flag) $(rust_test_options) $(rust_test_features_flag))

endif

ifdef HOST_RUST_LIBRARY_FILE

ifdef HOST_RUST_LIBRARY_FEATURES
host_rust_features_flag := --features '$(HOST_RUST_LIBRARY_FEATURES)'
endif

force-cargo-host-library-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) --lib $(cargo_host_flag) $(host_rust_features_flag)

$(HOST_RUST_LIBRARY_FILE): force-cargo-host-library-build ;

force-cargo-host-library-check:
	$(call CARGO_CHECK) --lib $(cargo_host_flag) $(host_rust_features_flag)
else
force-cargo-host-library-check:
	@true
endif # HOST_RUST_LIBRARY_FILE

ifdef RUST_PROGRAMS

force-cargo-program-build: $(call resfile,module)
	$(REPORT_BUILD)
	$(call CARGO_BUILD) $(addprefix --bin ,$(RUST_CARGO_PROGRAMS)) $(cargo_target_flag) -- $(addprefix -C link-arg=$(CURDIR)/,$(call resfile,module)) $(CARGO_RUSTCFLAGS)

$(RUST_PROGRAMS): force-cargo-program-build ;

force-cargo-program-check:
	$(call CARGO_CHECK) $(addprefix --bin ,$(RUST_CARGO_PROGRAMS)) $(cargo_target_flag)
else
force-cargo-program-check:
	@true
endif # RUST_PROGRAMS
ifdef HOST_RUST_PROGRAMS

force-cargo-host-program-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) $(addprefix --bin ,$(HOST_RUST_CARGO_PROGRAMS)) $(cargo_host_flag)

$(HOST_RUST_PROGRAMS): force-cargo-host-program-build ;

force-cargo-host-program-check:
	$(REPORT_BUILD)
	$(call CARGO_CHECK) $(addprefix --bin ,$(HOST_RUST_CARGO_PROGRAMS)) $(cargo_host_flag)
else
force-cargo-host-program-check:
	@true
endif # HOST_RUST_PROGRAMS
