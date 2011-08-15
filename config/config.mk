#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Benjamin Smedberg <benjamin@smedbergs.us>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# config.mk
#
# Determines the platform and builds the macros needed to load the
# appropriate platform-specific .mk file, then defines all (most?)
# of the generic macros.
#

# Define an include-at-most-once flag
ifdef INCLUDED_CONFIG_MK
$(error Don't include config.mk twice!)
endif
INCLUDED_CONFIG_MK = 1

EXIT_ON_ERROR = set -e; # Shell loops continue past errors without this.

ifndef topsrcdir
topsrcdir	= $(DEPTH)
endif

ifndef INCLUDED_AUTOCONF_MK
include $(DEPTH)/config/autoconf.mk
endif

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

nullstr :=
space :=$(nullstr) # EOL

core_winabspath = $(firstword $(subst /, ,$(call core_abspath,$(1)))):$(subst $(space),,$(patsubst %,\\%,$(wordlist 2,$(words $(subst /, ,$(call core_abspath,$(1)))), $(strip $(subst /, ,$(call core_abspath,$(1)))))))

# FINAL_TARGET specifies the location into which we copy end-user-shipped
# build products (typelibs, components, chrome).
#
# It will usually be the well-loved $(DIST)/bin, today, but can also be an
# XPI-contents staging directory for ambitious and right-thinking extensions.
FINAL_TARGET = $(if $(XPI_NAME),$(DIST)/xpi-stage/$(XPI_NAME),$(DIST)/bin)

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

ifeq ($(OS_ARCH),QNX)
ifeq ($(OS_TARGET),NTO)
LD		:= qcc -Vgcc_ntox86 -nostdlib
else
LD		:= $(CC)
endif
endif

#
# Strip off the excessively long version numbers on these platforms,
# but save the version to allow multiple versions of the same base
# platform to be built in the same tree.
#
ifneq (,$(filter FreeBSD HP-UX IRIX Linux NetBSD OpenBSD OSF1 SunOS,$(OS_ARCH)))
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

ifdef MOZ_MEMORY
ifneq ($(OS_ARCH),WINNT)
JEMALLOC_LIBS = $(MKSHLIB_FORCE_ALL) $(call EXPAND_MOZLIBNAME,jemalloc) $(MKSHLIB_UNFORCE_ALL)
# If we are linking jemalloc into a program, we want the jemalloc symbols
# to be exported
ifneq (,$(SIMPLE_PROGRAMS)$(PROGRAM))
JEMALLOC_LIBS += $(MOZ_JEMALLOC_STANDALONE_GLUE_LDOPTS)
endif
endif
endif

CC := $(CC_WRAPPER) $(CC)
CXX := $(CXX_WRAPPER) $(CXX)
MKDIR ?= mkdir
SLEEP ?= sleep
TOUCH ?= touch

ifndef .PYMAKE
PYTHON_PATH = $(PYTHON) $(topsrcdir)/config/pythonpath.py
else
PYCOMMANDPATH += $(topsrcdir)/config
PYTHON_PATH = %pythonpath main
endif

# determine debug-related options
_DEBUG_CFLAGS :=
_DEBUG_LDFLAGS :=

ifdef MOZ_DEBUG
  _DEBUG_CFLAGS += $(MOZ_DEBUG_ENABLE_DEFS) $(MOZ_DEBUG_FLAGS)
  _DEBUG_LDFLAGS += $(MOZ_DEBUG_LDFLAGS)
  XULPPFLAGS += $(MOZ_DEBUG_ENABLE_DEFS)
else
  _DEBUG_CFLAGS += $(MOZ_DEBUG_DISABLE_DEFS)
  XULPPFLAGS += $(MOZ_DEBUG_DISABLE_DEFS)
  ifdef MOZ_DEBUG_SYMBOLS
    _DEBUG_CFLAGS += $(MOZ_DEBUG_FLAGS)
    _DEBUG_LDFLAGS += $(MOZ_DEBUG_LDFLAGS)
  endif
endif

MOZALLOC_LIB = $(call EXPAND_LIBNAME_PATH,mozalloc,$(DIST)/lib)

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
OS_CXXFLAGS += -Zi -UDEBUG -DNDEBUG
OS_CFLAGS += -Zi -UDEBUG -DNDEBUG
ifdef HAVE_64BIT_OS
OS_LDFLAGS += -DEBUG -OPT:REF,ICF
else
OS_LDFLAGS += -DEBUG -OPT:REF
endif
endif

ifdef MOZ_QUANTIFY
# -FIXED:NO is needed for Quantify to work, but it increases the size
# of executables, so only use it if building for Quantify.
WIN32_EXE_LDFLAGS += -FIXED:NO

# We need -OPT:NOICF to prevent identical methods from being merged together.
# Otherwise, Quantify doesn't know which method was actually called when it's
# showing you the profile.
OS_LDFLAGS += -OPT:NOICF
endif

#
# Handle trace-malloc in optimized builds.
# No opt to give sane callstacks.
#
ifdef NS_TRACE_MALLOC
MOZ_OPTIMIZE_FLAGS=-Zi -Od -UDEBUG -DNDEBUG
ifdef HAVE_64BIT_OS
OS_LDFLAGS = -DEBUG -PDB:NONE -OPT:REF,ICF
else
OS_LDFLAGS = -DEBUG -PDB:NONE -OPT:REF
endif
endif # NS_TRACE_MALLOC

endif # MOZ_DEBUG

# We don't build a static CRT when building a custom CRT,
# it appears to be broken. So don't link to jemalloc if
# the Makefile wants static CRT linking.
ifeq ($(MOZ_MEMORY)_$(USE_STATIC_LIBS),1_)
# Disable default CRT libs and add the right lib path for the linker
OS_LDFLAGS += $(MOZ_MEMORY_LDFLAGS)
endif

endif # WINNT && !GNU_CC

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

ifeq ($(OS_ARCH),BSD_OS)
TAR_CREATE_FLAGS = -cvLf
endif

ifeq ($(OS_ARCH),OS2)
TAR_CREATE_FLAGS = -cvf
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
PURIFY = purify $(PURIFYOPTIONS)
QUANTIFY = quantify $(QUANTIFYOPTIONS)
ifdef CROSS_COMPILE
XPIDL_COMPILE = $(LIBXUL_DIST)/host/bin/host_xpidl$(HOST_BIN_SUFFIX)
else
XPIDL_COMPILE = $(LIBXUL_DIST)/bin/xpidl$(BIN_SUFFIX)
endif
XPIDL_LINK = $(PYTHON) $(LIBXUL_DIST)/sdk/bin/xpt.py link

# Java macros
JAVA_GEN_DIR  = _javagen
JAVA_DIST_DIR = $(DEPTH)/$(JAVA_GEN_DIR)
JAVA_IFACES_PKG_NAME = org/mozilla/interfaces

INCLUDES = \
  $(LOCAL_INCLUDES) \
  -I$(srcdir) \
  -I. \
  -I$(DIST)/include -I$(DIST)/include/nsprpub \
  $(if $(LIBXUL_SDK),-I$(LIBXUL_SDK)/include -I$(LIBXUL_SDK)/include/nsprpub) \
  $(OS_INCLUDES) \
  $(NULL) 

include $(topsrcdir)/config/static-checking-config.mk

CFLAGS		= $(OS_CFLAGS)
CXXFLAGS	= $(OS_CXXFLAGS)
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

# Check for FAIL_ON_WARNINGS & FAIL_ON_WARNINGS_DEBUG (Shorthand for Makefiles
# to request that we use the 'warnings as errors' compile flags)

# NOTE: First, we clear FAIL_ON_WARNINGS[_DEBUG] if we're doing a Windows PGO
# build, since WARNINGS_AS_ERRORS has been suspected of causing isuses in that
# situation. (See bug 437002.)
ifeq (WINNT_1,$(OS_ARCH)_$(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE))
FAIL_ON_WARNINGS_DEBUG=
FAIL_ON_WARNINGS=
endif # WINNT && (MOS_PROFILE_GENERATE ^ MOZ_PROFILE_USE)

# Also clear FAIL_ON_WARNINGS[_DEBUG] for Android builds, since
# they have some platform-specific warnings we haven't fixed yet.
ifeq ($(OS_TARGET),Android)
FAIL_ON_WARNINGS_DEBUG=
FAIL_ON_WARNINGS=
endif # Android

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
ifneq (,$(MOZ_DEBUG)$(NS_TRACE_MALLOC))
ifndef MOZ_NO_DEBUG_RTL
RTL_FLAGS=-MTd         # Statically linked multithreaded MSVC4.0 debug RTL
endif
endif # MOZ_DEBUG || NS_TRACE_MALLOC

else # !USE_STATIC_LIBS

RTL_FLAGS=-MD          # Dynamically linked, multithreaded RTL
ifneq (,$(MOZ_DEBUG)$(NS_TRACE_MALLOC))
ifndef MOZ_NO_DEBUG_RTL
RTL_FLAGS=-MDd         # Dynamically linked, multithreaded MSVC4.0 debug RTL
endif 
endif # MOZ_DEBUG || NS_TRACE_MALLOC
endif # USE_STATIC_LIBS
endif # WINNT && !GNU_CC

ifeq ($(OS_ARCH),Darwin)
# Darwin doesn't cross-compile, so just set both types of flags here.
HOST_CMFLAGS += -fobjc-exceptions
HOST_CMMFLAGS += -fobjc-exceptions
OS_COMPILE_CMFLAGS += -fobjc-exceptions
OS_COMPILE_CMMFLAGS += -fobjc-exceptions
endif

COMPILE_CFLAGS	= $(VISIBILITY_FLAGS) $(DEFINES) $(INCLUDES) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(CFLAGS) $(RTL_FLAGS) $(OS_COMPILE_CFLAGS)
COMPILE_CXXFLAGS = $(STL_FLAGS) $(VISIBILITY_FLAGS) $(DEFINES) $(INCLUDES) $(DSO_CFLAGS) $(DSO_PIC_CFLAGS) $(CXXFLAGS) $(RTL_FLAGS) $(OS_COMPILE_CXXFLAGS)
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

XPIDL_FLAGS = -I$(srcdir) -I$(IDL_DIR)
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

ifeq ($(OS_ARCH),Darwin)
ifdef NEXT_ROOT
export NEXT_ROOT
PBBUILD = NEXT_ROOT= $(PBBUILD_BIN)
else # NEXT_ROOT
PBBUILD = $(PBBUILD_BIN)
endif # NEXT_ROOT
PBBUILD_SETTINGS = GCC_VERSION="$(GCC_VERSION)" SYMROOT=build ARCHS="$(OS_TEST)"
ifdef MACOS_SDK_DIR
PBBUILD_SETTINGS += SDKROOT="$(MACOS_SDK_DIR)"
endif # MACOS_SDK_DIR
ifdef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET
PBBUILD_SETTINGS += MACOSX_DEPLOYMENT_TARGET="$(MACOSX_DEPLOYMENT_TARGET)"
endif # MACOSX_DEPLOYMENT_TARGET
ifdef MOZ_OPTIMIZE
ifeq (2,$(MOZ_OPTIMIZE))
# Only override project defaults if the config specified explicit settings
PBBUILD_SETTINGS += GCC_MODEL_TUNING= OPTIMIZATION_CFLAGS="$(MOZ_OPTIMIZE_FLAGS)"
endif # MOZ_OPTIMIZE=2
endif # MOZ_OPTIMIZE
endif # OS_ARCH=Darwin


ifdef MOZ_NATIVE_MAKEDEPEND
MKDEPEND_DIR =
MKDEPEND = $(MOZ_NATIVE_MAKEDEPEND)
else
MKDEPEND_DIR = $(CONFIG_TOOLS)/mkdepend
MKDEPEND = $(MKDEPEND_DIR)/mkdepend$(BIN_SUFFIX)
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
# Now test variables that might have been set or overridden by $(MY_CONFIG).

DEFINES		+= -DOSTYPE=\"$(OS_CONFIG)\"
DEFINES		+= -DOSARCH=$(OS_ARCH)

######################################################################

GARBAGE		+= $(DEPENDENCIES) $(MKDEPENDENCIES) $(MKDEPENDENCIES).bak core $(wildcard core.[0-9]*) $(wildcard *.err) $(wildcard *.pure) $(wildcard *_pure_*.o) Templates.DB

ifeq ($(OS_ARCH),Darwin)
ifndef NSDISTMODE
NSDISTMODE=absolute_symlink
endif
PWD := $(CURDIR)
endif

ifdef NSINSTALL_BIN
NSINSTALL = $(NSINSTALL_BIN)
else
ifeq (OS2,$(CROSS_COMPILE)$(OS_ARCH))
NSINSTALL = $(MOZ_TOOLS_DIR)/nsinstall
else
NSINSTALL = $(CONFIG_TOOLS)/nsinstall$(HOST_BIN_SUFFIX)
endif # OS2
endif # NSINSTALL_BIN


ifeq (,$(CROSS_COMPILE)$(filter-out WINNT OS2, $(OS_ARCH)))
INSTALL		= $(NSINSTALL)
else

# This isn't laid out as conditional directives so that NSDISTMODE can be
# target-specific.
INSTALL         = $(if $(filter copy, $(NSDISTMODE)), $(NSINSTALL) -t, $(if $(filter absolute_symlink, $(NSDISTMODE)), $(NSINSTALL) -L $(PWD), $(NSINSTALL) -R))

endif # WINNT/OS2

# Use nsinstall in copy mode to install files on the system
SYSINSTALL	= $(NSINSTALL) -t

# Directory nsinstall. Windows and OS/2 nsinstall can't recursively copy
# directories.
ifneq (,$(filter WINNT os2-emx,$(HOST_OS_ARCH)))
DIR_INSTALL = $(PYTHON) $(topsrcdir)/config/nsinstall.py
else
DIR_INSTALL = $(INSTALL)
endif # WINNT/OS2

#
# Localization build automation
#

# Because you might wish to "make locales AB_CD=ab-CD", we don't hardcode
# MOZ_UI_LOCALE directly, but use an intermediate variable that can be
# overridden by the command line. (Besides, AB_CD is prettier).
AB_CD = $(MOZ_UI_LOCALE)

ifndef L10NBASEDIR
L10NBASEDIR = $(error L10NBASEDIR not defined by configure)
endif

EXPAND_LOCALE_SRCDIR = $(if $(filter en-US,$(AB_CD)),$(topsrcdir)/$(1)/en-US,$(L10NBASEDIR)/$(AB_CD)/$(subst /locales,,$(1)))

ifdef relativesrcdir
LOCALE_SRCDIR = $(call EXPAND_LOCALE_SRCDIR,$(relativesrcdir))
endif

ifdef LOCALE_SRCDIR
# if LOCALE_MERGEDIR is set, use mergedir first, then the localization,
# and finally en-US
ifdef LOCALE_MERGEDIR
MAKE_JARS_FLAGS += -c $(LOCALE_MERGEDIR)/$(subst /locales,,$(relativesrcdir))
endif
MAKE_JARS_FLAGS += -c $(LOCALE_SRCDIR)
ifdef LOCALE_MERGEDIR
MAKE_JARS_FLAGS += -c $(topsrcdir)/$(relativesrcdir)/en-US
endif
endif

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

ifdef TIERS
DIRS += $(foreach tier,$(TIERS),$(tier_$(tier)_dirs))
STATIC_DIRS += $(foreach tier,$(TIERS),$(tier_$(tier)_staticdirs))
endif

OPTIMIZE_JARS_CMD = $(PYTHON) $(call core_abspath,$(topsrcdir)/config/optimizejars.py)

CREATE_PRECOMPLETE_CMD = $(PYTHON) $(call core_abspath,$(topsrcdir)/config/createprecomplete.py)

EXPAND_LIBS = $(PYTHON) -I$(DEPTH)/config $(topsrcdir)/config/expandlibs.py
EXPAND_LIBS_EXEC = $(PYTHON) $(topsrcdir)/config/pythonpath.py -I$(DEPTH)/config $(topsrcdir)/config/expandlibs_exec.py
EXPAND_LIBS_GEN = $(PYTHON) $(topsrcdir)/config/pythonpath.py -I$(DEPTH)/config $(topsrcdir)/config/expandlibs_gen.py
EXPAND_AR = $(EXPAND_LIBS_EXEC) --extract -- $(AR)
EXPAND_CC = $(EXPAND_LIBS_EXEC) --uselist -- $(CC)
EXPAND_CCC = $(EXPAND_LIBS_EXEC) --uselist -- $(CCC)
EXPAND_LD = $(EXPAND_LIBS_EXEC) --uselist -- $(LD)
EXPAND_MKSHLIB = $(EXPAND_LIBS_EXEC) --uselist -- $(MKSHLIB)

ifdef STDCXX_COMPAT
CHECK_STDCXX = objdump -p $(1) | grep -e 'GLIBCXX_3\.4\.\(9\|[1-9][0-9]\)' > /dev/null && echo "TEST-UNEXPECTED-FAIL | | We don't want these libstdc++ symbols to be used:" && objdump -T $(1) | grep -e 'GLIBCXX_3\.4\.\(9\|[1-9][0-9]\)' && exit 1 || exit 0
endif
