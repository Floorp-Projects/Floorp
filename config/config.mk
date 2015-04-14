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
# Using $(firstword) may not be perfect. But it should be good enough for most
# scenarios.
_current_makefile = $(CURDIR)/$(firstword $(MAKEFILE_LIST))

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

# LIBXUL_DIST is not defined under js/src, thus we make it mean DIST there.
LIBXUL_DIST ?= $(DIST)

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
DEFINES += -DXPI_NAME=$(XPI_NAME)
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

#
# Strip off the excessively long version numbers on these platforms,
# but save the version to allow multiple versions of the same base
# platform to be built in the same tree.
#
ifneq (,$(filter FreeBSD HP-UX Linux NetBSD OpenBSD SunOS,$(OS_ARCH)))
OS_RELEASE	:= $(basename $(OS_RELEASE))

# Allow the user to ignore the OS_VERSION, which is usually irrelevant.
ifdef WANT_MOZILLA_CONFIG_OS_VERSION
OS_VERS		:= $(suffix $(OS_RELEASE))
OS_VERSION	:= $(shell echo $(OS_VERS) | sed 's/-.*//')
endif

endif

OS_CONFIG	:= $(OS_ARCH)$(OS_RELEASE)

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

# determine debug-related options
_DEBUG_ASFLAGS :=
_DEBUG_CFLAGS :=
_DEBUG_LDFLAGS :=

ifdef MOZ_DEBUG
  _DEBUG_CFLAGS += $(MOZ_DEBUG_ENABLE_DEFS)
  XULPPFLAGS += $(MOZ_DEBUG_ENABLE_DEFS)
else
  _DEBUG_CFLAGS += $(MOZ_DEBUG_DISABLE_DEFS)
  XULPPFLAGS += $(MOZ_DEBUG_DISABLE_DEFS)
endif

ifneq (,$(MOZ_DEBUG)$(MOZ_DEBUG_SYMBOLS))
  ifeq ($(AS),yasm)
    ifeq ($(OS_ARCH)_$(GNU_CC),WINNT_)
      _DEBUG_ASFLAGS += -g cv8
    else
      ifneq ($(OS_ARCH),Darwin)
        _DEBUG_ASFLAGS += -g dwarf2
      endif
    endif
  else
    _DEBUG_ASFLAGS += $(MOZ_DEBUG_FLAGS)
  endif
  _DEBUG_CFLAGS += $(MOZ_DEBUG_FLAGS)
  _DEBUG_LDFLAGS += $(MOZ_DEBUG_LDFLAGS)
endif

ASFLAGS += $(_DEBUG_ASFLAGS)
OS_CFLAGS += $(_DEBUG_CFLAGS)
OS_CXXFLAGS += $(_DEBUG_CFLAGS)
OS_LDFLAGS += $(_DEBUG_LDFLAGS)

# XXX: What does this? Bug 482434 filed for better explanation.
ifeq ($(OS_ARCH)_$(GNU_CC),WINNT_)
ifdef MOZ_DEBUG
ifneq (,$(MOZ_BROWSE_INFO)$(MOZ_BSCFILE))
OS_CFLAGS += -FR
OS_CXXFLAGS += -FR
endif
else # ! MOZ_DEBUG

# MOZ_DEBUG_SYMBOLS generates debug symbols in separate PDB files.
# Used for generating an optimized build with debugging symbols.
# Used in the Windows nightlies to generate symbols for crash reporting.
ifdef MOZ_DEBUG_SYMBOLS
OS_CXXFLAGS += -UDEBUG -DNDEBUG
OS_CFLAGS += -UDEBUG -DNDEBUG
ifdef HAVE_64BIT_BUILD
OS_LDFLAGS += -DEBUG -OPT:REF,ICF
else
OS_LDFLAGS += -DEBUG -OPT:REF
endif
endif

#
# Handle DMD in optimized builds.
# No opt to give sane callstacks.
#
ifdef MOZ_DMD
MOZ_OPTIMIZE_FLAGS=-Zi -Od -UDEBUG -DNDEBUG
ifdef HAVE_64BIT_BUILD
OS_LDFLAGS = -DEBUG -OPT:REF,ICF
else
OS_LDFLAGS = -DEBUG -OPT:REF
endif
endif # MOZ_DMD

endif # MOZ_DEBUG

endif # WINNT && !GNU_CC

ifdef MOZ_GLUE_IN_PROGRAM
DEFINES += -DMOZ_GLUE_IN_PROGRAM
endif

#
# Build using PIC by default
#
_ENABLE_PIC=1

# No sense in profiling tools
ifdef INTERNAL_TOOLS
NO_PROFILE_GUIDED_OPTIMIZE = 1
endif

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
OS_CFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_GEN_CFLAGS))
OS_CXXFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_GEN_CFLAGS))
OS_LDFLAGS += $(PROFILE_GEN_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_GENERATE

ifdef MOZ_PROFILE_USE
OS_CFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_USE_CFLAGS))
OS_CXXFLAGS += $(if $(filter $(notdir $<),$(notdir $(NO_PROFILE_GUIDED_OPTIMIZE))),,$(PROFILE_USE_CFLAGS))
OS_LDFLAGS += $(PROFILE_USE_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_USE
endif # NO_PROFILE_GUIDED_OPTIMIZE

MAKE_JARS_FLAGS = \
	-t $(topsrcdir) \
	-f $(MOZ_CHROME_FILE_FORMAT) \
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

# Java macros
JAVA_GEN_DIR  = _javagen
JAVA_DIST_DIR = $(DEPTH)/$(JAVA_GEN_DIR)
JAVA_IFACES_PKG_NAME = org/mozilla/interfaces

INCLUDES = \
  -I$(srcdir) \
  -I. \
  $(LOCAL_INCLUDES) \
  -I$(DIST)/include \
  $(NULL)

ifndef IS_GYP_DIR
# NSPR_CFLAGS and NSS_CFLAGS must appear ahead of the other flags to avoid Linux
# builds wrongly picking up system NSPR/NSS header files.
OS_INCLUDES := \
  $(if $(LIBXUL_SDK),-I$(LIBXUL_SDK)/include) \
  $(NSPR_CFLAGS) $(NSS_CFLAGS) \
  $(MOZ_JPEG_CFLAGS) \
  $(MOZ_PNG_CFLAGS) \
  $(MOZ_ZLIB_CFLAGS) \
  $(MOZ_PIXMAN_CFLAGS) \
  $(NULL)
endif

include $(MOZILLA_DIR)/config/static-checking-config.mk

CFLAGS		= $(OS_CPPFLAGS) $(OS_CFLAGS)
CXXFLAGS	= $(OS_CPPFLAGS) $(OS_CXXFLAGS)
LDFLAGS		= $(OS_LDFLAGS) $(MOZBUILD_LDFLAGS) $(MOZ_FIX_LINK_PATHS)

# Allow each module to override the *default* optimization settings
# by setting MODULE_OPTIMIZE_FLAGS if the developer has not given
# arguments to --enable-optimize
ifdef MOZ_OPTIMIZE
ifeq (1,$(MOZ_OPTIMIZE))
ifdef MODULE_OPTIMIZE_FLAGS
CFLAGS		+= $(MODULE_OPTIMIZE_FLAGS)
CXXFLAGS	+= $(MODULE_OPTIMIZE_FLAGS)
else
ifneq (,$(if $(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE),$(MOZ_PGO_OPTIMIZE_FLAGS)))
CFLAGS		+= $(MOZ_PGO_OPTIMIZE_FLAGS)
CXXFLAGS	+= $(MOZ_PGO_OPTIMIZE_FLAGS)
else
CFLAGS		+= $(MOZ_OPTIMIZE_FLAGS)
CXXFLAGS	+= $(MOZ_OPTIMIZE_FLAGS)
endif # neq (,$(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE))
endif # MODULE_OPTIMIZE_FLAGS
else
CFLAGS		+= $(MOZ_OPTIMIZE_FLAGS)
CXXFLAGS	+= $(MOZ_OPTIMIZE_FLAGS)
endif # MOZ_OPTIMIZE == 1
LDFLAGS		+= $(MOZ_OPTIMIZE_LDFLAGS)
endif # MOZ_OPTIMIZE

ifdef CROSS_COMPILE
HOST_CFLAGS	+= $(HOST_OPTIMIZE_FLAGS)
else
ifdef MOZ_OPTIMIZE
ifeq (1,$(MOZ_OPTIMIZE))
ifdef MODULE_OPTIMIZE_FLAGS
HOST_CFLAGS	+= $(MODULE_OPTIMIZE_FLAGS)
else
HOST_CFLAGS	+= $(MOZ_OPTIMIZE_FLAGS)
endif # MODULE_OPTIMIZE_FLAGS
else
HOST_CFLAGS	+= $(MOZ_OPTIMIZE_FLAGS)
endif # MOZ_OPTIMIZE == 1
endif # MOZ_OPTIMIZE
endif # CROSS_COMPILE

CFLAGS += $(MOZ_FRAMEPTR_FLAGS)
CXXFLAGS += $(MOZ_FRAMEPTR_FLAGS)

# Check for FAIL_ON_WARNINGS (Shorthand for Makefiles to request that we use
# the 'warnings as errors' compile flags)

# NOTE: First, we clear FAIL_ON_WARNINGS[_DEBUG] if we're doing a Windows PGO
# build, since WARNINGS_AS_ERRORS has been suspected of causing isuses in that
# situation. (See bug 437002.)
ifeq (WINNT_1,$(OS_ARCH)_$(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE))
FAIL_ON_WARNINGS=
endif # WINNT && (MOS_PROFILE_GENERATE ^ MOZ_PROFILE_USE)

# Check for normal version of flag, and add WARNINGS_AS_ERRORS if it's set to 1.
ifdef FAIL_ON_WARNINGS
# Never treat warnings as errors in clang-cl, because it warns about many more
# things than MSVC does.
ifndef CLANG_CL
CXXFLAGS += $(WARNINGS_AS_ERRORS)
CFLAGS   += $(WARNINGS_AS_ERRORS)
endif # CLANG_CL
endif # FAIL_ON_WARNINGS

ifeq ($(OS_ARCH)_$(GNU_CC),WINNT_)
#// Currently, unless USE_STATIC_LIBS is defined, the multithreaded
#// DLL version of the RTL is used...
#//
#//------------------------------------------------------------------------
ifdef USE_STATIC_LIBS
RTL_FLAGS=-MT          # Statically linked multithreaded RTL
ifdef MOZ_DEBUG
ifndef MOZ_NO_DEBUG_RTL
RTL_FLAGS=-MTd         # Statically linked multithreaded MSVC4.0 debug RTL
endif
endif # MOZ_DEBUG

else # !USE_STATIC_LIBS

RTL_FLAGS=-MD          # Dynamically linked, multithreaded RTL
ifdef MOZ_DEBUG
ifndef MOZ_NO_DEBUG_RTL
RTL_FLAGS=-MDd         # Dynamically linked, multithreaded MSVC4.0 debug RTL
endif
endif # MOZ_DEBUG
endif # USE_STATIC_LIBS
endif # WINNT && !GNU_CC

ifeq ($(OS_ARCH),Darwin)
# Compiling ObjC requires an Apple compiler anyway, so it's ok to set
# host CMFLAGS here.
HOST_CMFLAGS += -fobjc-exceptions
HOST_CMMFLAGS += -fobjc-exceptions
OS_COMPILE_CMFLAGS += -fobjc-exceptions
OS_COMPILE_CMMFLAGS += -fobjc-exceptions
ifeq ($(MOZ_WIDGET_TOOLKIT),uikit)
OS_COMPILE_CMFLAGS += -fobjc-abi-version=2 -fobjc-legacy-dispatch
OS_COMPILE_CMMFLAGS += -fobjc-abi-version=2 -fobjc-legacy-dispatch
endif
endif

COMPILE_CFLAGS	= $(VISIBILITY_FLAGS) $(DEFINES) $(INCLUDES) $(OS_INCLUDES) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(RTL_FLAGS) $(OS_CPPFLAGS) $(OS_COMPILE_CFLAGS) $(CFLAGS) $(MOZBUILD_CFLAGS) $(EXTRA_COMPILE_FLAGS)
COMPILE_CXXFLAGS = $(if $(DISABLE_STL_WRAPPING),,$(STL_FLAGS)) $(VISIBILITY_FLAGS) $(DEFINES) $(INCLUDES) $(OS_INCLUDES) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(RTL_FLAGS) $(OS_CPPFLAGS) $(OS_COMPILE_CXXFLAGS) $(CXXFLAGS) $(MOZBUILD_CXXFLAGS) $(EXTRA_COMPILE_FLAGS)
COMPILE_CMFLAGS = $(OS_COMPILE_CMFLAGS) $(MOZBUILD_CMFLAGS) $(EXTRA_COMPILE_FLAGS)
COMPILE_CMMFLAGS = $(OS_COMPILE_CMMFLAGS) $(MOZBUILD_CMMFLAGS) $(EXTRA_COMPILE_FLAGS)
ASFLAGS += $(EXTRA_ASSEMBLER_FLAGS)

ifndef CROSS_COMPILE
HOST_CFLAGS += $(RTL_FLAGS)
endif

#
# Name of the binary code directories
#
# Override defaults

# Default location of include files
ifndef LIBXUL_SDK
IDL_PARSER_DIR = $(topsrcdir)/xpcom/idl-parser
IDL_PARSER_CACHE_DIR = $(DEPTH)/xpcom/idl-parser
else
IDL_PARSER_DIR = $(LIBXUL_SDK)/sdk/bin
IDL_PARSER_CACHE_DIR = $(LIBXUL_SDK)/sdk/bin
endif

SDK_LIB_DIR = $(DIST)/sdk/lib
SDK_BIN_DIR = $(DIST)/sdk/bin

DEPENDENCIES	= .md

ifdef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET
endif # MACOSX_DEPLOYMENT_TARGET

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
# set stack to 2MB on x64 build.  See bug 582910
WIN32_EXE_LDFLAGS	+= -STACK:2097152
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
# For Pymake, wherever we use nsinstall.py we're also going to try to make it
# a native command where possible. Since native commands can't be used outside
# of single-line commands, we continue to provide INSTALL for general use.
# Single-line commands should be switched over to install_cmd.
NSINSTALL_NATIVECMD := %nsinstall nsinstall

ifdef NSINSTALL_BIN
NSINSTALL = $(NSINSTALL_BIN)
else
ifeq ($(HOST_OS_ARCH),WINNT)
NSINSTALL = $(NSINSTALL_PY)
else
NSINSTALL = $(DEPTH)/config/nsinstall$(HOST_BIN_SUFFIX)
endif # WINNT
endif # NSINSTALL_BIN


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
DEFINES += -DAB_CD=$(AB_CD)

ifndef L10NBASEDIR
  L10NBASEDIR = $(error L10NBASEDIR not defined by configure)
else
  IS_LANGUAGE_REPACK = 1
endif

EXPAND_LOCALE_SRCDIR = $(if $(filter en-US,$(AB_CD)),$(topsrcdir)/$(1)/en-US,$(or $(realpath $(L10NBASEDIR)),$(abspath $(L10NBASEDIR)))/$(AB_CD)/$(subst /locales,,$(1)))

ifdef relativesrcdir
LOCALE_SRCDIR ?= $(call EXPAND_LOCALE_SRCDIR,$(relativesrcdir))
endif

ifdef relativesrcdir
MAKE_JARS_FLAGS += --relativesrcdir=$(relativesrcdir)
ifneq (en-US,$(AB_CD))
ifdef LOCALE_MERGEDIR
MAKE_JARS_FLAGS += --locale-mergedir=$(LOCALE_MERGEDIR)
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

ifdef LOCALE_MERGEDIR
MERGE_FILE = $(firstword \
  $(wildcard $(LOCALE_MERGEDIR)/$(subst /locales,,$(relativesrcdir))/$(1)) \
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
RUN_TEST_PROGRAM = $(LIBXUL_DIST)/bin/run-mozilla.sh
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
EXPAND_LD = $(EXPAND_LIBS_EXEC) --uselist -- $(LD)
EXPAND_MKSHLIB_ARGS = --uselist
ifdef SYMBOL_ORDER
EXPAND_MKSHLIB_ARGS += --symbol-order $(SYMBOL_ORDER)
endif
EXPAND_MKSHLIB = $(EXPAND_LIBS_EXEC) $(EXPAND_MKSHLIB_ARGS) -- $(MKSHLIB)

ifneq (,$(MOZ_LIBSTDCXX_TARGET_VERSION)$(MOZ_LIBSTDCXX_HOST_VERSION))
ifneq ($(OS_ARCH),Darwin)
CHECK_STDCXX = @$(TOOLCHAIN_PREFIX)objdump -p $(1) | grep -e 'GLIBCXX_3\.4\.\(9\|[1-9][0-9]\)' > /dev/null && echo 'TEST-UNEXPECTED-FAIL | check_stdcxx | We do not want these libstdc++ symbols to be used:' && $(TOOLCHAIN_PREFIX)objdump -T $(1) | grep -e 'GLIBCXX_3\.4\.\(9\|[1-9][0-9]\)' && exit 1 || true
endif
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

DEFINES += -DNO_NSPR_10_SUPPORT
