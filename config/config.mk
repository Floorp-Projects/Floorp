#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# config.mk
#
# Determines the platform and builds the macros needed to load the
# appropriate platform-specific .mk file, then defines all (most?)
# of the generic macros.
#

# Define an include-at-most-once flag
ifdef INCLUDED_CONFIG_MK
$(error Do not include config.mk twice!)
endif
INCLUDED_CONFIG_MK = 1

EXIT_ON_ERROR = set -e; # Shell loops continue past errors without this.

ifndef topsrcdir
topsrcdir	= $(DEPTH)
endif

ifndef INCLUDED_AUTOCONF_MK
include $(DEPTH)/config/autoconf.mk
endif

-include $(DEPTH)/.mozconfig.mk

ifndef EXTERNALLY_MANAGED_MAKE_FILE
# Import the automatically generated backend file. If this file doesn't exist,
# the backend hasn't been properly configured. We want this to be a fatal
# error, hence not using "-include".
ifndef STANDALONE_MAKEFILE
GLOBAL_DEPS += backend.mk
include backend.mk
endif

endif

space = $(NULL) $(NULL)

# Include defs.mk files that can be found in $(srcdir)/$(DEPTH),
# $(srcdir)/$(DEPTH-1), $(srcdir)/$(DEPTH-2), etc., and $(srcdir)
# where $(DEPTH-1) is one level less of depth, $(DEPTH-2), two, etc.
# i.e. for DEPTH=../../.., DEPTH-1 is ../.. and DEPTH-2 is ..
# These defs.mk files are used to define variables in a directory
# and all its subdirectories, recursively.
__depth := $(subst /, ,$(DEPTH))
ifeq (.,$(__depth))
__depth :=
endif
$(foreach __d,$(__depth) .,$(eval __depth = $(wordlist 2,$(words $(__depth)),$(__depth))$(eval -include $(subst $(space),/,$(strip $(srcdir) $(__depth) defs.mk)))))

COMMA = ,

# Sanity check some variables
CHECK_VARS := \
 XPI_NAME \
 LIBRARY_NAME \
 MODULE \
 DEPTH \
 XPI_PKGNAME \
 INSTALL_EXTENSION_ID \
 SHARED_LIBRARY_NAME \
 SONAME \
 STATIC_LIBRARY_NAME \
 $(NULL)

# checks for internal spaces or trailing spaces in the variable
# named by $x
check-variable = $(if $(filter-out 0 1,$(words $($(x))z)),$(error Spaces are not allowed in $(x)))

$(foreach x,$(CHECK_VARS),$(check-variable))

ifndef INCLUDED_FUNCTIONS_MK
include $(MOZILLA_DIR)/config/makefiles/functions.mk
endif

RM = rm -f

# FINAL_TARGET specifies the location into which we copy end-user-shipped
# build products (typelibs, components, chrome). It may already be specified by
# a moz.build file.
#
# If XPI_NAME is set, the files will be shipped to $(DIST)/xpi-stage/$(XPI_NAME)
# instead of $(DIST)/bin. In both cases, if DIST_SUBDIR is set, the files will be
# shipped to a $(DIST_SUBDIR) subdirectory.
FINAL_TARGET ?= $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)$(DIST_SUBDIR:%=/%)
# Override the stored value for the check to make sure that the variable is not
# redefined in the Makefile.in value.
FINAL_TARGET_FROZEN := '$(FINAL_TARGET)'

ifdef XPI_NAME
ACDEFINES += -DXPI_NAME=$(XPI_NAME)
endif

# The VERSION_NUMBER is suffixed onto the end of the DLLs we ship.
VERSION_NUMBER		= 50

ifeq ($(HOST_OS_ARCH),WINNT)
  ifeq ($(MOZILLA_DIR),$(topsrcdir))
    win_srcdir := $(subst $(topsrcdir),$(WIN_TOP_SRC),$(srcdir))
  else
    # This means we're in comm-central's topsrcdir, so we need to adjust
    # WIN_TOP_SRC (which points to mozilla's topsrcdir) for the substitution
    # to win_srcdir.
		cc_WIN_TOP_SRC := $(WIN_TOP_SRC:%/mozilla=%)
    win_srcdir := $(subst $(topsrcdir),$(cc_WIN_TOP_SRC),$(srcdir))
  endif
  BUILD_TOOLS = $(WIN_TOP_SRC)/build/unix
else
  win_srcdir := $(srcdir)
  BUILD_TOOLS = $(MOZILLA_DIR)/build/unix
endif

CONFIG_TOOLS	= $(MOZ_BUILD_ROOT)/config
AUTOCONF_TOOLS	= $(MOZILLA_DIR)/build/autoconf

ifdef _MSC_VER
CC_WRAPPER ?= $(call py_action,cl)
CXX_WRAPPER ?= $(call py_action,cl)
endif # _MSC_VER

CC := $(CC_WRAPPER) $(CC)
CXX := $(CXX_WRAPPER) $(CXX)
MKDIR ?= mkdir
SLEEP ?= sleep
TOUCH ?= touch

PYTHON_PATH = $(PYTHON) $(topsrcdir)/config/pythonpath.py

#
# Build using PIC by default
#
_ENABLE_PIC=1

# Don't build SIMPLE_PROGRAMS with PGO, since they don't need it anyway,
# and we don't have the same build logic to re-link them in the second pass.
ifdef SIMPLE_PROGRAMS
NO_PROFILE_GUIDED_OPTIMIZE = 1
endif

# No sense in profiling unit tests
ifdef CPP_UNIT_TESTS
NO_PROFILE_GUIDED_OPTIMIZE = 1
endif

# Enable profile-based feedback
ifneq (1,$(NO_PROFILE_GUIDED_OPTIMIZE))
ifdef MOZ_PROFILE_GENERATE
PGO_CFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_GEN_CFLAGS))
PGO_LDFLAGS += $(PROFILE_GEN_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_GENERATE

ifdef MOZ_PROFILE_USE
PGO_CFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_USE_CFLAGS))
PGO_LDFLAGS += $(PROFILE_USE_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_USE
endif # NO_PROFILE_GUIDED_OPTIMIZE

MAKE_JARS_FLAGS = \
	-t $(topsrcdir) \
	-f $(MOZ_JAR_MAKER_FILE_FORMAT) \
	$(NULL)

ifdef USE_EXTENSION_MANIFEST
MAKE_JARS_FLAGS += -e
endif

TAR_CREATE_FLAGS = -chf

#
# Personal makefile customizations go in these optional make include files.
#
MY_CONFIG	:= $(DEPTH)/config/myconfig.mk
MY_RULES	:= $(DEPTH)/config/myrules.mk

#
# Default command macros; can be overridden in <arch>.mk.
#
CCC = $(CXX)

INCLUDES = \
  -I$(srcdir) \
  -I$(CURDIR) \
  $(LOCAL_INCLUDES) \
  -I$(ABS_DIST)/include \
  $(NULL)

include $(MOZILLA_DIR)/config/static-checking-config.mk

LDFLAGS		= $(COMPUTED_LDFLAGS) $(PGO_LDFLAGS) $(MK_LDFLAGS)

COMPILE_CFLAGS	= $(COMPUTED_CFLAGS) $(PGO_CFLAGS) $(_DEPEND_CFLAGS) $(MK_COMPILE_DEFINES)
COMPILE_CXXFLAGS = $(COMPUTED_CXXFLAGS) $(PGO_CFLAGS) $(_DEPEND_CFLAGS) $(MK_COMPILE_DEFINES)
COMPILE_CMFLAGS = $(OS_COMPILE_CMFLAGS) $(MOZBUILD_CMFLAGS)
COMPILE_CMMFLAGS = $(OS_COMPILE_CMMFLAGS) $(MOZBUILD_CMMFLAGS)
ASFLAGS = $(COMPUTED_ASFLAGS)
SFLAGS = $(COMPUTED_SFLAGS)

HOST_CFLAGS = $(COMPUTED_HOST_CFLAGS) $(_DEPEND_CFLAGS)
HOST_CXXFLAGS = $(COMPUTED_HOST_CXXFLAGS) $(_DEPEND_CFLAGS)
HOST_C_LDFLAGS = $(COMPUTED_HOST_C_LDFLAGS)
HOST_CXX_LDFLAGS = $(COMPUTED_HOST_CXX_LDFLAGS)

# We only add color flags if neither the flag to disable color
# (e.g. "-fno-color-diagnostics" nor a flag to control color
# (e.g. "-fcolor-diagnostics=never") is present.
define colorize_flags
ifeq (,$(filter $(COLOR_CFLAGS:-f%=-fno-%),$$(1))$(findstring $(COLOR_CFLAGS),$$(1)))
$(1) += $(COLOR_CFLAGS)
endif
endef

color_flags_vars := \
  COMPILE_CFLAGS \
  COMPILE_CXXFLAGS \
  COMPILE_CMFLAGS \
  COMPILE_CMMFLAGS \
  LDFLAGS \
  $(NULL)

ifdef MACH_STDOUT_ISATTY
ifdef COLOR_CFLAGS
# TODO Bug 1319166 - iTerm2 interprets some bytes  sequences as a
# request to show a print dialog. Don't enable color on iTerm2 until
# a workaround is in place.
ifneq ($(TERM_PROGRAM),iTerm.app)
$(foreach var,$(color_flags_vars),$(eval $(call colorize_flags,$(var))))
endif
endif
endif

#
# Name of the binary code directories
#
# Override defaults

DEPENDENCIES	= .md

ifdef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET
endif # MACOSX_DEPLOYMENT_TARGET

# Export to propagate to cl and submake for third-party code.
# Eventually, we'll want to just use -I.
ifdef INCLUDE
export INCLUDE
endif

# Export to propagate to link.exe and submake for third-party code.
# Eventually, we'll want to just use -LIBPATH.
ifdef LIB
export LIB
endif

ifdef MOZ_USING_CCACHE
ifdef CLANG_CXX
export CCACHE_CPP2=1
endif
endif

# Set link flags according to whether we want a console.
ifeq ($(OS_ARCH),WINNT)
ifdef MOZ_WINCONSOLE
ifeq ($(MOZ_WINCONSOLE),1)
WIN32_EXE_LDFLAGS	+= $(WIN32_CONSOLE_EXE_LDFLAGS)
else # MOZ_WINCONSOLE
WIN32_EXE_LDFLAGS	+= $(WIN32_GUI_EXE_LDFLAGS)
endif
else
# For setting subsystem version
WIN32_EXE_LDFLAGS	+= $(WIN32_CONSOLE_EXE_LDFLAGS)
endif
endif # WINNT

ifdef _MSC_VER
ifeq ($(CPU_ARCH),x86_64)
ifdef MOZ_ASAN
# ASan could have 3x stack memory usage of normal builds.
WIN32_EXE_LDFLAGS	+= -STACK:6291456
else
# set stack to 2MB on x64 build.  See bug 582910
WIN32_EXE_LDFLAGS	+= -STACK:2097152
endif
endif
endif

#
# Include any personal overrides the user might think are needed.
#
-include $(topsrcdir)/$(MOZ_BUILD_APP)/app-config.mk
-include $(MY_CONFIG)

######################################################################

GARBAGE		+= $(DEPENDENCIES) core $(wildcard core.[0-9]*) $(wildcard *.err) $(wildcard *.pure) $(wildcard *_pure_*.o) Templates.DB

ifeq ($(OS_ARCH),Darwin)
ifndef NSDISTMODE
NSDISTMODE=absolute_symlink
endif
PWD := $(CURDIR)
endif

NSINSTALL_PY := $(PYTHON) $(abspath $(MOZILLA_DIR)/config/nsinstall.py)
ifneq (,$(or $(filter WINNT,$(HOST_OS_ARCH)),$(if $(COMPILE_ENVIRONMENT),,1)))
NSINSTALL = $(NSINSTALL_PY)
else
NSINSTALL = $(DEPTH)/config/nsinstall$(HOST_BIN_SUFFIX)
endif # WINNT


ifeq (,$(CROSS_COMPILE)$(filter-out WINNT, $(OS_ARCH)))
INSTALL = $(NSINSTALL) -t

else

# This isn't laid out as conditional directives so that NSDISTMODE can be
# target-specific.
INSTALL         = $(if $(filter copy, $(NSDISTMODE)), $(NSINSTALL) -t, $(if $(filter absolute_symlink, $(NSDISTMODE)), $(NSINSTALL) -L $(PWD), $(NSINSTALL) -R))

endif # WINNT

# The default for install_cmd is simply INSTALL
install_cmd ?= $(INSTALL) $(1)

# Use nsinstall in copy mode to install files on the system
SYSINSTALL	= $(NSINSTALL) -t
# This isn't necessarily true, just here
sysinstall_cmd = install_cmd

#
# Localization build automation
#

# Because you might wish to "make locales AB_CD=ab-CD", we don't hardcode
# MOZ_UI_LOCALE directly, but use an intermediate variable that can be
# overridden by the command line. (Besides, AB_CD is prettier).
AB_CD = $(MOZ_UI_LOCALE)
# Many locales directories want this definition.
ACDEFINES += -DAB_CD=$(AB_CD)

ifndef L10NBASEDIR
  L10NBASEDIR = $(error L10NBASEDIR not defined by configure)
endif

EXPAND_LOCALE_SRCDIR = $(if $(filter en-US,$(AB_CD)),$(topsrcdir)/$(1)/en-US,$(or $(realpath $(L10NBASEDIR)),$(abspath $(L10NBASEDIR)))/$(AB_CD)/$(subst /locales,,$(1)))

ifdef relativesrcdir
LOCALE_SRCDIR ?= $(call EXPAND_LOCALE_SRCDIR,$(relativesrcdir))
endif

ifdef relativesrcdir
MAKE_JARS_FLAGS += --relativesrcdir=$(relativesrcdir)
ifneq (en-US,$(AB_CD))
ifdef IS_LANGUAGE_REPACK
MAKE_JARS_FLAGS += --locale-mergedir=$(REAL_LOCALE_MERGEDIR)
endif
ifdef IS_LANGUAGE_REPACK
MAKE_JARS_FLAGS += --l10n-base=$(L10NBASEDIR)/$(AB_CD)
endif
else
MAKE_JARS_FLAGS += -c $(LOCALE_SRCDIR)
endif # en-US
else
MAKE_JARS_FLAGS += -c $(LOCALE_SRCDIR)
endif # ! relativesrcdir

ifdef IS_LANGUAGE_REPACK
MERGE_FILE = $(firstword \
  $(wildcard $(REAL_LOCALE_MERGEDIR)/$(subst /locales,,$(relativesrcdir))/$(1)) \
  $(wildcard $(LOCALE_SRCDIR)/$(1)) \
  $(srcdir)/en-US/$(1) )
else
MERGE_FILE = $(LOCALE_SRCDIR)/$(1)
endif
MERGE_FILES = $(foreach f,$(1),$(call MERGE_FILE,$(f)))

# These marcros are similar to MERGE_FILE, but no merging, and en-US first.
# They're used for searchplugins, for example.
EN_US_OR_L10N_FILE = $(firstword \
  $(wildcard $(srcdir)/en-US/$(1)) \
  $(LOCALE_SRCDIR)/$(1) )
EN_US_OR_L10N_FILES = $(foreach f,$(1),$(call EN_US_OR_L10N_FILE,$(f)))

ifneq (WINNT,$(OS_ARCH))
RUN_TEST_PROGRAM = $(DIST)/bin/run-mozilla.sh
endif # ! WINNT

#
# Java macros
#

# Make sure any compiled classes work with at least JVM 1.4
JAVAC_FLAGS += -source 1.4

ifdef MOZ_DEBUG
JAVAC_FLAGS += -g
endif

CREATE_PRECOMPLETE_CMD = $(PYTHON) $(abspath $(MOZILLA_DIR)/config/createprecomplete.py)

# MDDEPDIR is the subdirectory where dependency files are stored
MDDEPDIR := .deps

EXPAND_LIBS_EXEC = $(PYTHON) $(MOZILLA_DIR)/config/expandlibs_exec.py
EXPAND_LIBS_GEN = $(PYTHON) $(MOZILLA_DIR)/config/expandlibs_gen.py
EXPAND_AR = $(EXPAND_LIBS_EXEC) --extract -- $(AR)
EXPAND_CC = $(EXPAND_LIBS_EXEC) --uselist -- $(CC)
EXPAND_CCC = $(EXPAND_LIBS_EXEC) --uselist -- $(CCC)
EXPAND_LINK = $(EXPAND_LIBS_EXEC) --uselist -- $(LINK)
EXPAND_MKSHLIB_ARGS = --uselist
ifdef SYMBOL_ORDER
EXPAND_MKSHLIB_ARGS += --symbol-order $(SYMBOL_ORDER)
endif
EXPAND_MKSHLIB = $(EXPAND_LIBS_EXEC) $(EXPAND_MKSHLIB_ARGS) -- $(MKSHLIB)

# $(call CHECK_SYMBOLS,lib,PREFIX,dep_name,test)
# Checks that the given `lib` doesn't contain dependency on symbols with a
# version starting with `PREFIX`_ and matching the `test`. `dep_name` is only
# used for the error message.
# `test` is an awk expression using the information in the variable `v` which
# contains a list of version items ([major, minor, ...]).
define CHECK_SYMBOLS
@$(TOOLCHAIN_PREFIX)readelf -sW $(1) | \
awk '$$8 ~ /@$(2)_/ { \
	split($$8,a,"@"); \
	split(a[2],b,"_"); \
	split(b[2],v,"."); \
	if ($(4)) { \
		if (!found) { \
			print "TEST-UNEXPECTED-FAIL | check_stdcxx | We do not want these $(3) symbol versions to be used:" \
		} \
		print " ",$$8; \
		found=1 \
	} \
} \
END { \
	if (found) { \
		exit(1) \
	} \
}'
endef

ifneq (,$(MOZ_LIBSTDCXX_TARGET_VERSION)$(MOZ_LIBSTDCXX_HOST_VERSION))
CHECK_STDCXX = $(call CHECK_SYMBOLS,$(1),GLIBCXX,libstdc++,v[1] > 3 || (v[1] == 3 && v[2] == 4 && v[3] > 16))
CHECK_GLIBC = $(call CHECK_SYMBOLS,$(1),GLIBC,libc,v[1] > 2 || (v[1] == 2 && v[2] > 12))
endif

ifeq (,$(filter $(OS_TARGET),WINNT Darwin))
CHECK_TEXTREL = @$(TOOLCHAIN_PREFIX)readelf -d $(1) | grep TEXTREL > /dev/null && echo 'TEST-UNEXPECTED-FAIL | check_textrel | We do not want text relocations in libraries and programs' || true
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),android)
# While this is very unlikely (libc being added by the compiler at the end
# of the linker command line), if libmozglue.so ends up after libc.so, all
# hell breaks loose, so better safe than sorry, and check it's actually the
# case.
CHECK_MOZGLUE_ORDER = @$(TOOLCHAIN_PREFIX)readelf -d $(1) | grep NEEDED | awk '{ libs[$$NF] = ++n } END { if (libs["[libmozglue.so]"] && libs["[libc.so]"] < libs["[libmozglue.so]"]) { print "libmozglue.so must be linked before libc.so"; exit 1 } }'
endif

define CHECK_BINARY
$(call CHECK_GLIBC,$(1))
$(call CHECK_STDCXX,$(1))
$(call CHECK_TEXTREL,$(1))
$(call LOCAL_CHECKS,$(1))
$(call CHECK_MOZGLUE_ORDER,$(1))
endef

# autoconf.mk sets OBJ_SUFFIX to an error to avoid use before including
# this file
OBJ_SUFFIX := $(_OBJ_SUFFIX)

# PGO builds with GCC build objects with instrumentation in a first pass,
# then objects optimized, without instrumentation, in a second pass. If
# we overwrite the objects from the first pass with those from the second,
# we end up not getting instrumentation data for better optimization on
# incremental builds. As a consequence, we use a different object suffix
# for the first pass.
ifndef NO_PROFILE_GUIDED_OPTIMIZE
ifdef MOZ_PROFILE_GENERATE
ifdef GNU_CC
OBJ_SUFFIX := i_o
endif
endif
endif

PLY_INCLUDE = -I$(MOZILLA_DIR)/other-licenses/ply

export CL_INCLUDES_PREFIX
# Make sure that the build system can handle non-ASCII characters
# in environment variables to prevent it from breking silently on
# non-English systems.
export NONASCII
