# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# /!\ In this file, we export multiple variables globally via make rather than
# in recipes via the `env` command to avoid round-trips to msys on Windows, which
# tend to break environment variable values in interesting ways.

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
# Enable link-time optimization for release builds.
cargo_rustc_flags += -C lto
endif
endif

ifdef CARGO_INCREMENTAL
export CARGO_INCREMENTAL
endif

rustflags_neon =
ifeq (neon,$(MOZ_FPU))
rustflags_neon += -C target_feature=+neon
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

ifndef MOZ_ASAN
ifndef MOZ_TSAN
ifndef MOZ_UBSAN
ifneq (1,$(CODE_COVERAGE_GCC))
ifndef FUZZING_INTERFACES
# Pass the compilers and flags in use to cargo for use in build scripts.
# * Don't do this for ASAN/TSAN builds because we don't pass our custom linker (see below)
#   which will muck things up.
# * Don't do this for GCC code coverage builds because the way rustc invokes the linker doesn't
#   work with GCC 6: https://bugzilla.mozilla.org/show_bug.cgi?id=1477305
#
# We don't pass HOST_{CC,CXX} down in any form because our host value might not match
# what cargo chooses and there's no way to control cargo's selection, so we just have to
# hope that if something needs to build a host C source file it can find a usable compiler!
#
# We're passing these for consumption by the `cc` crate, which doesn't use the same
# convention as cargo itself:
# https://github.com/alexcrichton/cc-rs/blob/baa71c0e298d9ad7ac30f0ad78f20b4b3b3a8fb2/src/lib.rs#L1715
rust_cc_env_name := $(subst -,_,$(RUST_TARGET))

ifeq (WINNT,$(HOST_OS_ARCH))
# Don't do most of this on Windows because msys path translation makes a mess of the paths, and
# we put MSVC in PATH there anyway.  But we do suppress warnings, since all such warnings
# are in third-party code.
export CFLAGS_$(rust_cc_env_name)=-w
else
export CC_$(rust_cc_env_name)=$(CC)
export CXX_$(rust_cc_env_name)=$(CXX)
export CFLAGS_$(rust_cc_env_name)=$(COMPUTED_CFLAGS)
export CXXFLAGS_$(rust_cc_env_name)=$(COMPUTED_CXXFLAGS)
export AR_$(rust_cc_env_name)=$(AR)
endif # WINNT
endif # FUZZING_INTERFACES
endif # MOZ_CODE_COVERAGE
endif # MOZ_UBSAN
endif # MOZ_TSAN
endif # MOZ_ASAN

export CARGO_TARGET_DIR
export RUSTFLAGS
export RUSTC
export RUSTDOC
export RUSTFMT
export MOZ_SRC=$(topsrcdir)
export MOZ_DIST=$(ABS_DIST)
export LIBCLANG_PATH=$(MOZ_LIBCLANG_PATH)
export CLANG_PATH=$(MOZ_CLANG_PATH)
export PKG_CONFIG_ALLOW_CROSS=1
export RUST_BACKTRACE=full
export MOZ_TOPOBJDIR=$(topobjdir)

TARGET_RECIPES := \
  force-cargo-test-run \
  $(foreach a,library program,$(foreach b,build check,force-cargo-$(a)-$(b)))

$(TARGET_RECIPES): RUSTFLAGS:=$(rustflags_override) $(RUSTFLAGS)

HOST_RECIPES := \
  $(foreach a,library program,$(foreach b,build check,force-cargo-host-$(a)-$(b)))

$(HOST_RECIPES): RUSTFLAGS:=$(rustflags_override)

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

cargo_linker_env_var := CARGO_TARGET_$(RUST_TARGET_ENV_NAME)_LINKER

# Don't define a custom linker on Windows, as it's difficult to have a
# non-binary file that will get executed correctly by Cargo.  We don't
# have to worry about a cross-compiling (besides x86-64 -> x86, which
# already works with the current setup) setup on Windows, and we don't
# have to pass in any special linker options on Windows.
ifneq (WINNT,$(OS_ARCH))

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
# Exporting from make always exports a value. Setting a value per-recipe
# would export an empty value for the host recipes. When not doing a
# cross-compile, the --target for those is the same, and cargo will use
# $(cargo_linker_env_var) for its linker, so we always pass the
# cargo-linker wrapper, and fill MOZ_CARGO_WRAP_LD* more or less
# appropriately for all recipes.
export $(cargo_linker_env_var):=$(topsrcdir)/build/cargo-linker
$(TARGET_RECIPES): MOZ_CARGO_WRAP_LDFLAGS:=$(filter-out -fsanitize=cfi% -framework Cocoa -lobjc AudioToolbox ExceptionHandling -fprofile-%,$(LDFLAGS))
$(TARGET_RECIPES): MOZ_CARGO_WRAP_LD:=$(CC)
$(HOST_RECIPES): MOZ_CARGO_WRAP_LDFLAGS:=$(HOST_LDFLAGS)
$(HOST_RECIPES): MOZ_CARGO_WRAP_LD:=$(HOST_CC)
endif # FUZZING_INTERFACES
endif # MOZ_UBSAN
endif # MOZ_TSAN
endif # MOZ_ASAN

endif # ifneq WINNT

ifdef RUST_LIBRARY_FILE

ifdef RUST_LIBRARY_FEATURES
rust_features_flag := --features "$(RUST_LIBRARY_FEATURES)"
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
ifndef DEVELOPER_OPTIONS
ifndef MOZ_DEBUG_RUST
ifeq ($(OS_ARCH), Linux)
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
rust_features_flag := --features "$(RUST_TEST_FEATURES)"
endif

# Don't stop at the first failure. We want to list all failures together.
rust_test_flag := --no-fail-fast

force-cargo-test-run:
	$(call RUN_CARGO,test $(cargo_target_flag) $(rust_test_flag) $(rust_test_options) $(rust_features_flag))

endif

ifdef HOST_RUST_LIBRARY_FILE

ifdef HOST_RUST_LIBRARY_FEATURES
host_rust_features_flag := --features "$(HOST_RUST_LIBRARY_FEATURES)"
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
force-cargo-program-build:
	$(REPORT_BUILD)
	$(call CARGO_BUILD) $(addprefix --bin ,$(RUST_CARGO_PROGRAMS)) $(cargo_target_flag)

$(RUST_PROGRAMS): force-cargo-program-build

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

$(HOST_RUST_PROGRAMS): force-cargo-host-program-build

force-cargo-host-program-check:
	$(REPORT_BUILD)
	$(call CARGO_CHECK) $(addprefix --bin ,$(HOST_RUST_CARGO_PROGRAMS)) $(cargo_host_flag)
else
force-cargo-host-program-check:
	@true
endif # HOST_RUST_PROGRAMS
