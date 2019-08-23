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
cargo_build_flags += --color=always
endif
endif

# These flags are passed via `cargo rustc` and only apply to the final rustc
# invocation (i.e., only the top-level crate, not its dependencies).
cargo_rustc_flags = $(CARGO_RUSTCFLAGS)
ifndef DEVELOPER_OPTIONS
ifndef MOZ_DEBUG_RUST
# Enable link-time optimization for release builds, but not when linking
# gkrust_gtest.
ifeq (,$(findstring gkrust_gtest,$(RUST_LIBRARY_FILE)))
cargo_rustc_flags += -Clto
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

rustflags_override = $(MOZ_RUST_DEFAULT_FLAGS) $(rustflags_neon)

ifdef MOZ_USING_SCCACHE
export RUSTC_WRAPPER=$(CCACHE)
endif

ifdef MOZ_CODE_COVERAGE
ifeq (gcc,$(CC_TYPE))
CODE_COVERAGE_GCC=1
endif
endif

# We start with host variables because the rust host and the rust target might be the same,
# in which case we want the latter to take priority.

# We're passing these for consumption by the `cc` crate, which doesn't use the same
# convention as cargo itself:
# https://github.com/alexcrichton/cc-rs/blob/baa71c0e298d9ad7ac30f0ad78f20b4b3b3a8fb2/src/lib.rs#L1715
rust_host_cc_env_name := $(subst -,_,$(RUST_HOST_TARGET))

export CC_$(rust_host_cc_env_name)=$(HOST_CC)
export CXX_$(rust_host_cc_env_name)=$(HOST_CXX)
# We don't have a HOST_AR. If rust needs one, assume it's going to pick an appropriate one.

rust_cc_env_name := $(subst -,_,$(RUST_TARGET))

export CC_$(rust_cc_env_name)=$(CC)
export CXX_$(rust_cc_env_name)=$(CXX)
export AR_$(rust_cc_env_name)=$(AR)
ifeq (,$(MOZ_ASAN)$(MOZ_TSAN)$(MOZ_UBSAN)$(CODE_COVERAGE_GCC)$(FUZZING_INTERFACES))
# -DMOZILLA_CONFIG_H is added to prevent mozilla-config.h from injecting anything
# in C/C++ compiles from rust. That's not needed in the other branch because the
# base flags don't force-include mozilla-config.h.
export CFLAGS_$(rust_host_cc_env_name)=$(COMPUTED_HOST_CFLAGS) -DMOZILLA_CONFIG_H
export CXXFLAGS_$(rust_host_cc_env_name)=$(COMPUTED_HOST_CXXFLAGS) -DMOZILLA_CONFIG_H
export CFLAGS_$(rust_cc_env_name)=$(COMPUTED_CFLAGS) -DMOZILLA_CONFIG_H
export CXXFLAGS_$(rust_cc_env_name)=$(COMPUTED_CXXFLAGS) -DMOZILLA_CONFIG_H
else
# Because cargo doesn't allow to distinguish builds happening for build
# scripts/procedural macros vs. those happening for the rust target,
# we can't blindly pass all our flags down for cc-rs to use them, because of the
# side effects they can have on what otherwise should be host builds.
# So for sanitizer, fuzzing and coverage builds, we only pass the base compiler
# flags.
# This means C code built by rust is not going to be covered by sanitizer,
# fuzzing and coverage. But at least we control what compiler is being used,
# rather than relying on cc-rs guesses, which, sometimes fail us.
export CFLAGS_$(rust_host_cc_env_name)=$(HOST_CC_BASE_FLAGS)
export CXXFLAGS_$(rust_host_cc_env_name)=$(HOST_CXX_BASE_FLAGS)
export CFLAGS_$(rust_cc_env_name)=$(CC_BASE_FLAGS)
export CXXFLAGS_$(rust_cc_env_name)=$(CXX_BASE_FLAGS)
endif

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
export RUST_BACKTRACE=full
export MOZ_TOPOBJDIR=$(topobjdir)

target_rust_ltoable := force-cargo-library-build
target_rust_nonltoable := force-cargo-test-run force-cargo-library-check $(foreach b,build check,force-cargo-program-$(b))

ifdef MOZ_PGO_RUST
rust_pgo_flags := $(if $(MOZ_PROFILE_GENERATE),-C profile-generate=$(topobjdir)) $(if $(MOZ_PROFILE_USE),-C profile-use=$(topobjdir)/merged.profdata)
endif

$(target_rust_ltoable): RUSTFLAGS:=$(rustflags_override) $(RUSTFLAGS) $(if $(MOZ_LTO_RUST),-Clinker-plugin-lto) $(rust_pgo_flags)
$(target_rust_nonltoable): RUSTFLAGS:=$(rustflags_override) $(RUSTFLAGS)

TARGET_RECIPES := $(target_rust_ltoable) $(target_rust_nonltoable)

HOST_RECIPES := \
  $(foreach a,library program,$(foreach b,build check,force-cargo-host-$(a)-$(b)))

$(HOST_RECIPES): RUSTFLAGS:=$(rustflags_override)

cargo_env = $(subst -,_,$(subst a,A,$(subst b,B,$(subst c,C,$(subst d,D,$(subst e,E,$(subst f,F,$(subst g,G,$(subst h,H,$(subst i,I,$(subst j,J,$(subst k,K,$(subst l,L,$(subst m,M,$(subst n,N,$(subst o,O,$(subst p,P,$(subst q,Q,$(subst r,R,$(subst s,S,$(subst t,T,$(subst u,U,$(subst v,V,$(subst w,W,$(subst x,X,$(subst y,Y,$(subst z,Z,$1)))))))))))))))))))))))))))

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

cargo_host_linker_env_var := CARGO_TARGET_$(call cargo_env,$(RUST_HOST_TARGET))_LINKER
cargo_linker_env_var := CARGO_TARGET_$(call cargo_env,$(RUST_TARGET))_LINKER

# Defining all of this for ASan/TSan builds results in crashes while running
# some crates's build scripts (!), so disable it for now.
ifndef MOZ_ASAN
ifndef MOZ_TSAN
ifndef MOZ_UBSAN
ifndef FUZZING_INTERFACES
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
$(TARGET_RECIPES): MOZ_CARGO_WRAP_LDFLAGS:=$(filter-out -fsanitize=cfi% -framework Cocoa -lobjc AudioToolbox ExceptionHandling -fprofile-%,$(LDFLAGS))

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

endif # FUZZING_INTERFACES
endif # MOZ_UBSAN
endif # MOZ_TSAN
endif # MOZ_ASAN

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
# The check only works when rust code is built with -Clto.
ifndef MOZ_PROFILE_GENERATE
ifeq ($(OS_ARCH), Linux)
ifneq (,$(filter -Clto,$(cargo_rustc_flags)))
	$(call py_action,check_binary,--target --networking $@)
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

$(HOST_RUST_LIBRARY_FILE): force-cargo-host-library-build

force-cargo-host-library-check:
	$(call CARGO_CHECK) --lib $(cargo_host_flag) $(host_rust_features_flag)
else
force-cargo-host-library-check:
	@true
endif # HOST_RUST_LIBRARY_FILE

ifdef RUST_PROGRAMS

GARBAGE_DIRS += $(RUST_TARGET)

force-cargo-program-build: $(RESFILE)
	$(REPORT_BUILD)
	$(call CARGO_BUILD) $(addprefix --bin ,$(RUST_CARGO_PROGRAMS)) $(cargo_target_flag) -- $(if $(RESFILE),-C link-arg=$(CURDIR)/$(RESFILE))

$(RUST_PROGRAMS): force-cargo-program-build

force-cargo-program-check:
	$(call CARGO_CHECK) $(addprefix --bin ,$(RUST_CARGO_PROGRAMS)) $(cargo_target_flag)
else
force-cargo-program-check:
	@true
endif # RUST_PROGRAMS
ifdef HOST_RUST_PROGRAMS

GARBAGE_DIRS += $(RUST_HOST_TARGET)

force-cargo-host-program-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) $(addprefix --bin ,$(HOST_RUST_CARGO_PROGRAMS)) $(cargo_host_flag)

$(HOST_RUST_PROGRAMS): force-cargo-host-program-build

force-cargo-host-program-check:
	$(REPORT_BUILD)
	$(call CARGO_CHECK) $(addprefix --bin ,$(HOST_RUST_CARGO_PROGRAMS)) $(cargo_host_flag)
else
force-cargo-host-program-check:
	@true
endif # HOST_RUST_PROGRAMS
