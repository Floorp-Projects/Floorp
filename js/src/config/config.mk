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
 SHORT_LIBNAME \
 XPI_PKGNAME \
 INSTALL_EXTENSION_ID \
 SHARED_LIBRARY_NAME \
 STATIC_LIBRARY_NAME \
 $(NULL)

# checks for internal spaces or trailing spaces in the variable
# named by $x
check-variable = $(if $(filter-out 0 1,$(words $($(x))z)),$(error Spaces are not allowed in $(x)))

$(foreach x,$(CHECK_VARS),$(check-variable))

core_abspath = $(if $(findstring :,$(1)),$(1),$(if $(filter /%,$(1)),$(1),$(CURDIR)/$(1)))
core_realpath = $(if $(realpath $(1)),$(realpath $(1)),$(call core_abspath,$(1)))

core_winabspath = $(firstword $(subst /, ,$(call core_abspath,$(1)))):$(subst $(space),,$(patsubst %,\\%,$(wordlist 2,$(words $(subst /, ,$(call core_abspath,$(1)))), $(strip $(subst /, ,$(call core_abspath,$(1)))))))

RM = rm -f

# LIBXUL_DIST is not defined under js/src, thus we make it mean DIST there.
LIBXUL_DIST ?= $(DIST)

# FINAL_TARGET specifies the location into which we copy end-user-shipped
# build products (typelibs, components, chrome).
#
# If XPI_NAME is set, the files will be shipped to $(DIST)/xpi-stage/$(XPI_NAME)
# instead of $(DIST)/bin. In both cases, if DIST_SUBDIR is set, the files will be
# shipped to a $(DIST_SUBDIR) subdirectory.
FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)$(DIST_SUBDIR:%=/%)

ifdef XPI_NAME
DEFINES += -DXPI_NAME=$(XPI_NAME)
endif

# The VERSION_NUMBER is suffixed onto the end of the DLLs we ship.
VERSION_NUMBER		= 50

ifeq ($(HOST_OS_ARCH),WINNT)
win_srcdir	:= $(subst $(topsrcdir),$(WIN_TOP_SRC),$(srcdir))
BUILD_TOOLS	= $(WIN_TOP_SRC)/build/unix
else
win_srcdir	:= $(srcdir)
BUILD_TOOLS	= $(topsrcdir)/build/unix
endif

CONFIG_TOOLS	= $(MOZ_BUILD_ROOT)/config
AUTOCONF_TOOLS	= $(topsrcdir)/build/autoconf

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

FINAL_LINK_LIBS = $(DEPTH)/config/final-link-libs
FINAL_LINK_COMPS = $(DEPTH)/config/final-link-comps
FINAL_LINK_COMP_NAMES = $(DEPTH)/config/final-link-comp-names

MOZ_UNICHARUTIL_LIBS = $(LIBXUL_DIST)/lib/$(LIB_PREFIX)unicharutil_s.$(LIB_SUFFIX)
MOZ_WIDGET_SUPPORT_LIBS    = $(DIST)/lib/$(LIB_PREFIX)widgetsupport_s.$(LIB_SUFFIX)

ifdef _MSC_VER
CC_WRAPPER ?= $(PYTHON) -O $(topsrcdir)/build/cl.py
CXX_WRAPPER ?= $(PYTHON) -O $(topsrcdir)/build/cl.py
endif # _MSC_VER

CC := $(CC_WRAPPER) $(CC)
CXX := $(CXX_WRAPPER) $(CXX)
MKDIR ?= mkdir
SLEEP ?= sleep
TOUCH ?= touch

ifdef .PYMAKE
PYCOMMANDPATH += $(topsrcdir)/config
endif

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

MOZALLOC_LIB = $(call EXPAND_LIBNAME_PATH,mozalloc,$(DIST)/lib)

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
ifdef HAVE_64BIT_OS
OS_LDFLAGS += -DEBUG -OPT:REF,ICF
else
OS_LDFLAGS += -DEBUG -OPT:REF
endif
endif

#
# Handle trace-malloc and DMD in optimized builds.
# No opt to give sane callstacks.
#
ifneq (,$(NS_TRACE_MALLOC)$(MOZ_DMD))
MOZ_OPTIMIZE_FLAGS=-Zi -Od -UDEBUG -DNDEBUG
ifdef HAVE_64BIT_OS
OS_LDFLAGS = -DEBUG -OPT:REF,ICF
else
OS_LDFLAGS = -DEBUG -OPT:REF
endif
endif # NS_TRACE_MALLOC || MOZ_DMD

endif # MOZ_DEBUG

# We don't build a static CRT when building a custom CRT,
# it appears to be broken. So don't link to jemalloc if
# the Makefile wants static CRT linking.
ifeq ($(MOZ_MEMORY)_$(USE_STATIC_LIBS),1_1)
# Disable default CRT libs and add the right lib path for the linker
MOZ_GLUE_LDFLAGS=
endif

endif # WINNT && !GNU_CC

ifdef MOZ_GLUE_PROGRAM_LDFLAGS
DEFINES += -DMOZ_GLUE_IN_PROGRAM
else
MOZ_GLUE_PROGRAM_LDFLAGS=$(MOZ_GLUE_LDFLAGS)
endif

#
# Build using PIC by default
#
_ENABLE_PIC=1

# Determine if module being compiled is destined
# to be merged into libxul

ifdef LIBXUL_LIBRARY
ifdef IS_COMPONENT
ifdef MODULE_NAME
DEFINES += -DXPCOM_TRANSLATE_NSGM_ENTRY_POINT=1
else
$(error Component makefile does not specify MODULE_NAME.)
endif
endif
FORCE_STATIC_LIB=1
SHORT_LIBNAME=
endif

# If we are building this component into an extension/xulapp, it cannot be
# statically linked. In the future we may want to add a xulapp meta-component
# build option.

ifdef XPI_NAME
ifdef IS_COMPONENT
EXPORT_LIBRARY=
FORCE_STATIC_LIB=
FORCE_SHARED_LIB=1
endif
endif

ifndef SHARED_LIBRARY_NAME
ifdef LIBRARY_NAME
SHARED_LIBRARY_NAME=$(LIBRARY_NAME)
endif
endif

ifndef STATIC_LIBRARY_NAME
ifdef LIBRARY_NAME
STATIC_LIBRARY_NAME=$(LIBRARY_NAME)
endif
endif

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
ifndef NO_PROFILE_GUIDED_OPTIMIZE
ifdef MOZ_PROFILE_GENERATE
OS_CFLAGS += $(PROFILE_GEN_CFLAGS)
OS_CXXFLAGS += $(PROFILE_GEN_CFLAGS)
OS_LDFLAGS += $(PROFILE_GEN_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_GENERATE

ifdef MOZ_PROFILE_USE
OS_CFLAGS += $(PROFILE_USE_CFLAGS)
OS_CXXFLAGS += $(PROFILE_USE_CFLAGS)
OS_LDFLAGS += $(PROFILE_USE_LDFLAGS)
ifeq (WINNT,$(OS_ARCH))
AR_FLAGS += -LTCG
endif
endif # MOZ_PROFILE_USE
endif # NO_PROFILE_GUIDED_OPTIMIZE


# Does the makefile specifies the internal XPCOM API linkage?
ifneq (,$(MOZILLA_INTERNAL_API)$(LIBXUL_LIBRARY))
DEFINES += -DMOZILLA_INTERNAL_API
endif

# Force XPCOM/widget/gfx methods to be _declspec(dllexport) when we're
# building libxul libraries
ifdef LIBXUL_LIBRARY
DEFINES += \
		-D_IMPL_NS_COM \
		-DEXPORT_XPT_API \
		-DEXPORT_XPTC_API \
		-D_IMPL_NS_GFX \
		-D_IMPL_NS_WIDGET \
		-DIMPL_XREAPI \
		-DIMPL_NS_NET \
		-DIMPL_THEBES \
		$(NULL)

ifndef JS_SHARED_LIBRARY
DEFINES += -DSTATIC_EXPORTABLE_JS_API
endif
endif

# Flags passed to JarMaker.py
MAKE_JARS_FLAGS = \
	-t $(topsrcdir) \
	-f $(MOZ_CHROME_FILE_FORMAT) \
	$(NULL)

ifdef USE_EXTENSION_MANIFEST
MAKE_JARS_FLAGS += -e
endif

ifdef BOTH_MANIFESTS
MAKE_JARS_FLAGS += --both-manifests
endif

TAR_CREATE_FLAGS = -cvhf
TAR_CREATE_FLAGS_QUIET = -chf

ifeq ($(OS_ARCH),OS2)
TAR_CREATE_FLAGS = -cvf
TAR_CREATE_FLAGS_QUIET = -cf
endif

#
# Personal makefile customizations go in these optional make include files.
#
MY_CONFIG	:= $(DEPTH)/config/myconfig.mk
MY_RULES	:= $(DEPTH)/config/myrules.mk

#
# Default command macros; can be overridden in <arch>.mk.
#
CCC = $(CXX)
XPIDL_LINK = $(PYTHON) $(LIBXUL_DIST)/sdk/bin/xpt.py link

# Java macros
JAVA_GEN_DIR  = _javagen
JAVA_DIST_DIR = $(DEPTH)/$(JAVA_GEN_DIR)
JAVA_IFACES_PKG_NAME = org/mozilla/interfaces

OS_INCLUDES += $(MOZ_JPEG_CFLAGS) $(MOZ_PNG_CFLAGS) $(MOZ_ZLIB_CFLAGS)

# NSPR_CFLAGS and NSS_CFLAGS must appear ahead of OS_INCLUDES to avoid Linux
# builds wrongly picking up system NSPR/NSS header files.
INCLUDES = \
  $(LOCAL_INCLUDES) \
  -I$(srcdir) \
  -I. \
  -I$(DIST)/include \
  $(if $(LIBXUL_SDK),-I$(LIBXUL_SDK)/include) \
  $(NSPR_CFLAGS) $(NSS_CFLAGS) \
  $(OS_INCLUDES) \
  $(NULL)

include $(topsrcdir)/config/static-checking-config.mk

CFLAGS		= $(OS_CPPFLAGS) $(OS_CFLAGS)
CXXFLAGS	= $(OS_CPPFLAGS) $(OS_CXXFLAGS)
LDFLAGS		= $(OS_LDFLAGS) $(MOZ_FIX_LINK_PATHS)

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

# Check for FAIL_ON_WARNINGS & FAIL_ON_WARNINGS_DEBUG (Shorthand for Makefiles
# to request that we use the 'warnings as errors' compile flags)

# NOTE: First, we clear FAIL_ON_WARNINGS[_DEBUG] if we're doing a Windows PGO
# build, since WARNINGS_AS_ERRORS has been suspected of causing isuses in that
# situation. (See bug 437002.)
ifeq (WINNT_1,$(OS_ARCH)_$(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE))
FAIL_ON_WARNINGS_DEBUG=
FAIL_ON_WARNINGS=
endif # WINNT && (MOS_PROFILE_GENERATE ^ MOZ_PROFILE_USE)

# Now, check for debug version of flag; it turns on normal flag in debug builds.
ifdef FAIL_ON_WARNINGS_DEBUG
ifdef MOZ_DEBUG
FAIL_ON_WARNINGS = 1
endif # MOZ_DEBUG
endif # FAIL_ON_WARNINGS_DEBUG

# Check for normal version of flag, and add WARNINGS_AS_ERRORS if it's set to 1.
ifdef FAIL_ON_WARNINGS
CXXFLAGS += $(WARNINGS_AS_ERRORS)
CFLAGS   += $(WARNINGS_AS_ERRORS)
endif # FAIL_ON_WARNINGS

ifeq ($(OS_ARCH)_$(GNU_CC),WINNT_)
#// Currently, unless USE_STATIC_LIBS is defined, the multithreaded
#// DLL version of the RTL is used...
#//
#//------------------------------------------------------------------------
ifdef USE_STATIC_LIBS
RTL_FLAGS=-MT          # Statically linked multithreaded RTL
ifneq (,$(MOZ_DEBUG)$(NS_TRACE_MALLOC)$(MOZ_DMD))
ifndef MOZ_NO_DEBUG_RTL
RTL_FLAGS=-MTd         # Statically linked multithreaded MSVC4.0 debug RTL
endif
endif # MOZ_DEBUG || NS_TRACE_MALLOC || MOZ_DMD

else # !USE_STATIC_LIBS

RTL_FLAGS=-MD          # Dynamically linked, multithreaded RTL
ifneq (,$(MOZ_DEBUG)$(NS_TRACE_MALLOC)$(MOZ_DMD))
ifndef MOZ_NO_DEBUG_RTL
RTL_FLAGS=-MDd         # Dynamically linked, multithreaded MSVC4.0 debug RTL
endif
endif # MOZ_DEBUG || NS_TRACE_MALLOC || MOZ_DMD
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

COMPILE_CFLAGS	= $(VISIBILITY_FLAGS) $(DEFINES) $(INCLUDES) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(CFLAGS) $(RTL_FLAGS) $(OS_CPPFLAGS) $(OS_COMPILE_CFLAGS)
COMPILE_CXXFLAGS = $(STL_FLAGS) $(VISIBILITY_FLAGS) $(DEFINES) $(INCLUDES) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(CXXFLAGS) $(RTL_FLAGS) $(OS_CPPFLAGS) $(OS_COMPILE_CXXFLAGS)
COMPILE_CMFLAGS = $(OS_COMPILE_CMFLAGS)
COMPILE_CMMFLAGS = $(OS_COMPILE_CMMFLAGS)

ifndef CROSS_COMPILE
HOST_CFLAGS += $(RTL_FLAGS)
endif

#
# Name of the binary code directories
#
# Override defaults

# We need to know where to find the libraries we
# put on the link line for binaries, and should
# we link statically or dynamic?  Assuming dynamic for now.

ifneq (WINNT_,$(OS_ARCH)_$(GNU_CC))
LIBS_DIR	= -L$(DIST)/bin -L$(DIST)/lib
ifdef LIBXUL_SDK
LIBS_DIR	+= -L$(LIBXUL_SDK)/bin -L$(LIBXUL_SDK)/lib
endif
endif

# Default location of include files
IDL_DIR		= $(DIST)/idl

XPIDL_FLAGS += -I$(srcdir) -I$(IDL_DIR)
ifdef LIBXUL_SDK
XPIDL_FLAGS += -I$(LIBXUL_SDK)/idl
endif

SDK_LIB_DIR = $(DIST)/sdk/lib
SDK_BIN_DIR = $(DIST)/sdk/bin

DEPENDENCIES	= .md

MOZ_COMPONENT_LIBS=$(XPCOM_LIBS) $(MOZ_COMPONENT_NSPR_LIBS)

ifeq ($(OS_ARCH),OS2)
ELF_DYNSTR_GC	= echo
else
ELF_DYNSTR_GC	= :
endif

ifndef CROSS_COMPILE
ifdef USE_ELF_DYNSTR_GC
ifdef MOZ_COMPONENTS_VERSION_SCRIPT_LDFLAGS
ELF_DYNSTR_GC 	= $(DEPTH)/config/elf-dynstr-gc
endif
endif
endif

ifdef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET
endif # MACOSX_DEPLOYMENT_TARGET

ifdef MOZ_USING_CCACHE
ifdef CLANG_CXX
export CCACHE_CPP2=1
endif
endif

# Set link flags according to whether we want a console.
ifdef MOZ_WINCONSOLE
ifeq ($(MOZ_WINCONSOLE),1)
ifeq ($(OS_ARCH),OS2)
BIN_FLAGS	+= -Zlinker -PM:VIO
endif
ifeq ($(OS_ARCH),WINNT)
ifdef GNU_CC
WIN32_EXE_LDFLAGS	+= -mconsole
else
WIN32_EXE_LDFLAGS	+= -SUBSYSTEM:CONSOLE
endif
endif
else # MOZ_WINCONSOLE
ifeq ($(OS_ARCH),OS2)
BIN_FLAGS	+= -Zlinker -PM:PM
endif
ifeq ($(OS_ARCH),WINNT)
ifdef GNU_CC
WIN32_EXE_LDFLAGS	+= -mwindows
else
WIN32_EXE_LDFLAGS	+= -SUBSYSTEM:WINDOWS
endif
endif
endif
endif

ifdef _MSC_VER
ifeq ($(CPU_ARCH),x86_64)
# set stack to 2MB on x64 build.  See bug 582910
WIN32_EXE_LDFLAGS	+= -STACK:2097152
endif
endif

# If we're building a component on MSVC, we don't want to generate an
# import lib, because that import lib will collide with the name of a
# static version of the same library.
ifeq ($(GNU_LD)$(OS_ARCH),WINNT)
ifdef IS_COMPONENT
LDFLAGS += -IMPLIB:fake.lib
DELETE_AFTER_LINK = fake.lib fake.exp
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

NSINSTALL_PY := $(PYTHON) $(call core_abspath,$(topsrcdir)/config/nsinstall.py)
# For Pymake, wherever we use nsinstall.py we're also going to try to make it
# a native command where possible. Since native commands can't be used outside
# of single-line commands, we continue to provide INSTALL for general use.
# Single-line commands should be switched over to install_cmd.
NSINSTALL_NATIVECMD := %nsinstall nsinstall

ifdef NSINSTALL_BIN
NSINSTALL = $(NSINSTALL_BIN)
else
ifeq (OS2,$(CROSS_COMPILE)$(OS_ARCH))
NSINSTALL = $(MOZ_TOOLS_DIR)/nsinstall
else
ifeq ($(HOST_OS_ARCH),WINNT)
NSINSTALL = $(NSINSTALL_PY)
else
NSINSTALL = $(CONFIG_TOOLS)/nsinstall$(HOST_BIN_SUFFIX)
endif # WINNT
endif # OS2
endif # NSINSTALL_BIN


ifeq (,$(CROSS_COMPILE)$(filter-out WINNT OS2, $(OS_ARCH)))
INSTALL = $(NSINSTALL) -t
ifdef .PYMAKE
install_cmd = $(NSINSTALL_NATIVECMD) -t $(1)
endif # .PYMAKE

else

# This isn't laid out as conditional directives so that NSDISTMODE can be
# target-specific.
INSTALL         = $(if $(filter copy, $(NSDISTMODE)), $(NSINSTALL) -t, $(if $(filter absolute_symlink, $(NSDISTMODE)), $(NSINSTALL) -L $(PWD), $(NSINSTALL) -R))

endif # WINNT/OS2

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

ifndef L10NBASEDIR
  L10NBASEDIR = $(error L10NBASEDIR not defined by configure)
else
  IS_LANGUAGE_REPACK = 1
endif

EXPAND_LOCALE_SRCDIR = $(if $(filter en-US,$(AB_CD)),$(topsrcdir)/$(1)/en-US,$(call core_realpath,$(L10NBASEDIR))/$(AB_CD)/$(subst /locales,,$(1)))

ifdef relativesrcdir
LOCALE_SRCDIR = $(call EXPAND_LOCALE_SRCDIR,$(relativesrcdir))
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

ifeq (OS2,$(OS_ARCH))
RUN_TEST_PROGRAM = $(topsrcdir)/build/os2/test_os2.cmd "$(LIBXUL_DIST)"
else
ifneq (WINNT,$(OS_ARCH))
RUN_TEST_PROGRAM = $(LIBXUL_DIST)/bin/run-mozilla.sh
endif # ! WINNT
endif # ! OS2

#
# Java macros
#

# Make sure any compiled classes work with at least JVM 1.4
JAVAC_FLAGS += -source 1.4

ifdef MOZ_DEBUG
JAVAC_FLAGS += -g
endif

CREATE_PRECOMPLETE_CMD = $(PYTHON) $(call core_abspath,$(topsrcdir)/config/createprecomplete.py)

# MDDEPDIR is the subdirectory where dependency files are stored
MDDEPDIR := .deps

EXPAND_LIBS_EXEC = $(PYTHON) $(topsrcdir)/config/expandlibs_exec.py $(if $@,--depend $(MDDEPDIR)/$(@F).pp --target $@)
EXPAND_LIBS_GEN = $(PYTHON) $(topsrcdir)/config/expandlibs_gen.py $(if $@,--depend $(MDDEPDIR)/$(@F).pp)
EXPAND_AR = $(EXPAND_LIBS_EXEC) --extract -- $(AR)
EXPAND_CC = $(EXPAND_LIBS_EXEC) --uselist -- $(CC)
EXPAND_CCC = $(EXPAND_LIBS_EXEC) --uselist -- $(CCC)
EXPAND_LD = $(EXPAND_LIBS_EXEC) --uselist -- $(LD)
EXPAND_MKSHLIB_ARGS = --uselist
ifdef SYMBOL_ORDER
EXPAND_MKSHLIB_ARGS += --symbol-order $(SYMBOL_ORDER)
endif
EXPAND_MKSHLIB = $(EXPAND_LIBS_EXEC) $(EXPAND_MKSHLIB_ARGS) -- $(MKSHLIB)

ifdef STDCXX_COMPAT
ifneq ($(OS_ARCH),Darwin)
CHECK_STDCXX = objdump -p $(1) | grep -e 'GLIBCXX_3\.4\.\(9\|[1-9][0-9]\)' > /dev/null && echo "TEST-UNEXPECTED-FAIL | | We don't want these libstdc++ symbols to be used:" && objdump -T $(1) | grep -e 'GLIBCXX_3\.4\.\(9\|[1-9][0-9]\)' && exit 1 || exit 0
endif

EXTRA_LIBS += $(call EXPAND_LIBNAME_PATH,stdc++compat,$(DEPTH)/build/unix/stdc++compat)
HOST_EXTRA_LIBS += $(call EXPAND_LIBNAME_PATH,host_stdc++compat,$(DEPTH)/build/unix/stdc++compat)
endif

# autoconf.mk sets OBJ_SUFFIX to an error to avoid use before including
# this file
OBJ_SUFFIX := $(_OBJ_SUFFIX)

# PGO builds with GCC build objects with instrumentation in a first pass,
# then objects optimized, without instrumentation, in a second pass. If
# we overwrite the ojects from the first pass with those from the second,
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

# EXPAND_LIBNAME - $(call EXPAND_LIBNAME,foo)
# expands to $(LIB_PREFIX)foo.$(LIB_SUFFIX) or -lfoo, depending on linker
# arguments syntax. Should only be used for system libraries

# EXPAND_LIBNAME_PATH - $(call EXPAND_LIBNAME_PATH,foo,dir)
# expands to dir/$(LIB_PREFIX)foo.$(LIB_SUFFIX)

# EXPAND_MOZLIBNAME - $(call EXPAND_MOZLIBNAME,foo)
# expands to $(DIST)/lib/$(LIB_PREFIX)foo.$(LIB_SUFFIX)

ifdef GNU_CC
EXPAND_LIBNAME = $(addprefix -l,$(1))
else
EXPAND_LIBNAME = $(foreach lib,$(1),$(LIB_PREFIX)$(lib).$(LIB_SUFFIX))
endif
EXPAND_LIBNAME_PATH = $(foreach lib,$(1),$(2)/$(LIB_PREFIX)$(lib).$(LIB_SUFFIX))
EXPAND_MOZLIBNAME = $(foreach lib,$(1),$(DIST)/lib/$(LIB_PREFIX)$(lib).$(LIB_SUFFIX))

# Include internal ply only if needed
ifndef MOZ_SYSTEM_PLY
PLY_INCLUDE = -I$(topsrcdir)/other-licenses/ply
endif

export CL_INCLUDES_PREFIX

ifeq ($(MOZ_WIDGET_GTK),2)
MOZ_GTK2_CFLAGS := -I$(topsrcdir)/widget/gtk2/compat $(MOZ_GTK2_CFLAGS)
endif

DEFINES += -DNO_NSPR_10_SUPPORT
