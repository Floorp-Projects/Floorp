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

# MDDEPDIR is the subdirectory where dependency files are stored
MDDEPDIR := .deps

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

CONFIG_TOOLS	= $(MOZ_BUILD_ROOT)/config
AUTOCONF_TOOLS	= $(MOZILLA_DIR)/build/autoconf

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
PGO_CFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_GEN_CFLAGS) $(COMPUTED_PROFILE_GEN_DYN_CFLAGS))
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

LOCALE_TOPDIR ?= $(topsrcdir)
MAKE_JARS_FLAGS = \
	-t $(LOCALE_TOPDIR) \
	-f $(MOZ_JAR_MAKER_FILE_FORMAT) \
	$(NULL)

ifdef USE_EXTENSION_MANIFEST
MAKE_JARS_FLAGS += -e
endif

TAR_CREATE_FLAGS = -chf

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

ifndef MOZ_LTO
MOZ_LTO_CFLAGS :=
MOZ_LTO_LDFLAGS :=
endif

LDFLAGS		= $(MOZ_LTO_LDFLAGS) $(COMPUTED_LDFLAGS) $(PGO_LDFLAGS) $(MK_LDFLAGS)

COMPILE_CFLAGS	= $(MOZ_LTO_CFLAGS) $(COMPUTED_CFLAGS) $(PGO_CFLAGS) $(_DEPEND_CFLAGS) $(MK_COMPILE_DEFINES)
COMPILE_CXXFLAGS = $(MOZ_LTO_CFLAGS) $(COMPUTED_CXXFLAGS) $(PGO_CFLAGS) $(_DEPEND_CFLAGS) $(MK_COMPILE_DEFINES)
COMPILE_CMFLAGS = $(MOZ_LTO_CFLAGS) $(OS_COMPILE_CMFLAGS) $(MOZBUILD_CMFLAGS)
COMPILE_CMMFLAGS = $(MOZ_LTO_CFLAGS) $(OS_COMPILE_CMMFLAGS) $(MOZBUILD_CMMFLAGS)
ASFLAGS = $(COMPUTED_ASFLAGS)
SFLAGS = $(COMPUTED_SFLAGS)

HOST_CFLAGS = $(COMPUTED_HOST_CFLAGS) $(_DEPEND_CFLAGS)
HOST_CXXFLAGS = $(COMPUTED_HOST_CXXFLAGS) $(_DEPEND_CFLAGS)
HOST_C_LDFLAGS = $(COMPUTED_HOST_C_LDFLAGS)
HOST_CXX_LDFLAGS = $(COMPUTED_HOST_CXX_LDFLAGS)

ifdef MOZ_LTO
ifeq (Darwin,$(OS_TARGET))
# When linking on macOS, debug info is not linked along with the final binary,
# and the dwarf data stays in object files until they are "linked" with the
# dsymutil tool.
# With LTO, object files are temporary, and are not kept around, which
# means there's no object file for dsymutil to do its job. Consequently,
# there is no debug info for LTOed compilation units.
# The macOS linker has however an option to explicitly keep those object
# files, which dsymutil will then find.
# The catch is that the linker uses sequential numbers for those object
# files, and doesn't avoid conflicts from multiple linkers running at
# the same time. So in directories with multiple binaries, object files
# from the first linked binaries would be overwritten by those of the
# last linked binary. So we use a subdirectory containing the name of the
# linked binary.
LDFLAGS += -Wl,-object_path_lto,$(@F).lto.o/
endif
endif

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

ifdef CCACHE_PREFIX
export CCACHE_PREFIX
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

ifneq (,$(filter msvc clang-cl,$(CC_TYPE)))
ifneq ($(CPU_ARCH),x86)
# Normal operation on 64-bit Windows needs 2 MB of stack. (Bug 582910)
# ASAN requires 6 MB of stack.
# Setting the stack to 8 MB to match the capability of other systems
# to deal with frame construction for unreasonably deep DOM trees
# with worst-case styling. This uses address space unnecessarily for
# non-main threads, but that should be tolerable on 64-bit systems.
# (Bug 256180)
WIN32_EXE_LDFLAGS      += -STACK:8388608
else
# Since this setting affects the default stack size for non-main
# threads, too, to avoid burning the address space, increase only
# 512 KB over the default. Just enough to be able to deal with
# reasonable styling applied to DOM trees whose depth is near what
# Blink's HTML parser can output, esp.
# layout/base/crashtests/507119.html (Bug 256180)
WIN32_EXE_LDFLAGS      += -STACK:1572864
endif
endif

-include $(topsrcdir)/$(MOZ_BUILD_APP)/app-config.mk

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
INSTALL         = $(if $(filter absolute_symlink, $(NSDISTMODE)), $(NSINSTALL) -L $(PWD), $(NSINSTALL) -R)

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

include $(MOZILLA_DIR)/config/AB_rCD.mk

# Many locales directories want this definition.
ACDEFINES += -DAB_CD=$(AB_CD)

EXPAND_LOCALE_SRCDIR = $(if $(filter en-US,$(AB_CD)),$(LOCALE_TOPDIR)/$(1)/en-US,$(or $(realpath $(L10NBASEDIR)),$(abspath $(L10NBASEDIR)))/$(AB_CD)/$(subst /locales,,$(1)))

ifdef relativesrcdir
LOCALE_RELATIVEDIR ?= $(relativesrcdir)
endif

ifdef LOCALE_RELATIVEDIR
LOCALE_SRCDIR ?= $(call EXPAND_LOCALE_SRCDIR,$(LOCALE_RELATIVEDIR))
endif

ifdef relativesrcdir
MAKE_JARS_FLAGS += --relativesrcdir=$(LOCALE_RELATIVEDIR)
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
  $(wildcard $(REAL_LOCALE_MERGEDIR)/$(subst /locales,,$(LOCALE_RELATIVEDIR))/$(1)) \
  $(wildcard $(LOCALE_SRCDIR)/$(1)) \
  $(srcdir)/en-US/$(1) )
# Like MERGE_FILE, but with the specified relative source directory
# $(2) replacing $(srcdir).  It's expected that $(2) will include
# '/locales' but not '/locales/en-US'.
#
# MERGE_RELATIVE_FILE and MERGE_FILE could be -- ahem -- merged by
# making the second argument optional, but that expression makes for
# difficult to read Make.
MERGE_RELATIVE_FILE = $(firstword \
  $(wildcard $(REAL_LOCALE_MERGEDIR)/$(subst /locales,,$(2))/$(1)) \
  $(wildcard $(call EXPAND_LOCALE_SRCDIR,$(2))/$(1)) \
  $(topsrcdir)/$(2)/en-US/$(1) )
else
MERGE_FILE = $(LOCALE_SRCDIR)/$(1)
MERGE_RELATIVE_FILE = $(call EXPAND_LOCALE_SRCDIR,$(2))/$(1)
endif

ifneq (WINNT,$(OS_ARCH))
RUN_TEST_PROGRAM = $(DIST)/bin/run-mozilla.sh
endif # ! WINNT

# autoconf.mk sets OBJ_SUFFIX to an error to avoid use before including
# this file
OBJ_SUFFIX := $(_OBJ_SUFFIX)

OBJS_VAR_SUFFIX := OBJS

# PGO builds with GCC and clang-cl build objects with instrumentation in
# a first pass, then objects optimized, without instrumentation, in a
# second pass. If we overwrite the objects from the first pass with
# those from the second, we end up not getting instrumentation data for
# better optimization on incremental builds. As a consequence, we use a
# different object suffix for the first pass.
ifdef MOZ_PROFILE_GENERATE
ifneq (,$(GNU_CC)$(CLANG_CL))
OBJS_VAR_SUFFIX := PGO_OBJS
ifndef NO_PROFILE_GUIDED_OPTIMIZE
OBJ_SUFFIX := i_o
endif
endif
endif

PLY_INCLUDE = -I$(MOZILLA_DIR)/other-licenses/ply

# Enable verbose logs when not using `make -s`
ifeq (,$(findstring s, $(filter-out --%, $(MAKEFLAGS))))
BUILD_VERBOSE_LOG = 1
endif
