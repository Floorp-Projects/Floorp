# This file is normally included by autoconf.mk, but it is also used
# directly in python/mozbuild/mozbuild/base.py for gmake validation.
# We thus use INCLUDED_AUTOCONF_MK to enable/disable some parts depending
# whether a normal build is happening or whether the check is running.
includedir := $(includedir)/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
idldir = $(datadir)/idl/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
installdir = $(libdir)/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
sdkdir = $(libdir)/$(MOZ_APP_NAME)-devel-$(MOZ_APP_VERSION)
ifndef TOP_DIST
TOP_DIST = dist
endif
ifneq (,$(filter /%,$(TOP_DIST)))
DIST = $(TOP_DIST)
else
ifeq (.,$(DEPTH))
DIST = $(TOP_DIST)
else
DIST = $(DEPTH)/$(TOP_DIST)
endif
endif

# We do magic with OBJ_SUFFIX in config.mk, the following ensures we don't
# manually use it before config.mk inclusion
_OBJ_SUFFIX := $(OBJ_SUFFIX)
OBJ_SUFFIX = $(error config/config.mk needs to be included before using OBJ_SUFFIX)

ifeq ($(HOST_OS_ARCH),WINNT)
# We only support building with a non-msys gnu make version
# strictly above 4.0.
ifdef .PYMAKE
$(error Pymake is no longer supported. Please upgrade to MozillaBuild 1.9 or newer and build with 'mach' or 'mozmake')
endif

ifeq (a,$(firstword a$(subst /, ,$(abspath .))))
$(error MSYS make is not supported)
endif
# 4.0- happens to be greater than 4.0, lower than the mozmake version,
# and lower than 4.0.1 or 4.1, whatever next version of gnu make will
# be released.
ifneq (4.0-,$(firstword $(sort 4.0- $(MAKE_VERSION))))
$(error Make version too old. Only versions strictly greater than 4.0 are supported.)
endif

ifdef INCLUDED_AUTOCONF_MK
ifeq (a,$(firstword a$(subst /, ,$(srcdir))))
$(error MSYS-style srcdir are not supported for Windows builds.)
endif
endif
endif # WINNT

include_deps = $(eval $(if $(2),,-)include $(1))

ifndef INCLUDED_AUTOCONF_MK
default::
else

# Integrate with mozbuild-generated make files. We first verify that no
# variables provided by the automatically generated .mk files are
# present. If they are, this is a violation of the separation of
# responsibility between Makefile.in and mozbuild files.
_MOZBUILD_EXTERNAL_VARIABLES := \
  ANDROID_GENERATED_RESFILES \
  ANDROID_RES_DIRS \
  CMSRCS \
  CMMSRCS \
  CPP_UNIT_TESTS \
  DIRS \
  EXTRA_DSO_LDOPTS \
  EXTRA_JS_MODULES \
  EXTRA_PP_COMPONENTS \
  EXTRA_PP_JS_MODULES \
  FORCE_SHARED_LIB \
  FORCE_STATIC_LIB \
  FINAL_LIBRARY \
  HOST_CSRCS \
  HOST_CMMSRCS \
  HOST_EXTRA_LIBS \
  HOST_LIBRARY_NAME \
  HOST_PROGRAM \
  HOST_SIMPLE_PROGRAMS \
  IS_COMPONENT \
  JAR_MANIFEST \
  JAVA_JAR_TARGETS \
  LD_VERSION_SCRIPT \
  LIBRARY_NAME \
  LIBS \
  MAKE_FRAMEWORK \
  MODULE \
  MSVC_ENABLE_PGO \
  NO_DIST_INSTALL \
  OS_LIBS \
  PARALLEL_DIRS \
  PREF_JS_EXPORTS \
  PROGRAM \
  PYTHON_UNIT_TESTS \
  RESOURCE_FILES \
  SDK_HEADERS \
  SDK_LIBRARY \
  SHARED_LIBRARY_LIBS \
  SHARED_LIBRARY_NAME \
  SIMPLE_PROGRAMS \
  SONAME \
  STATIC_LIBRARY_NAME \
  TEST_DIRS \
  TOOL_DIRS \
  XPCSHELL_TESTS \
  XPIDL_MODULE \
  $(NULL)

_DEPRECATED_VARIABLES := \
  ANDROID_RESFILES \
  EXPORT_LIBRARY \
  EXTRA_LIBS \
  HOST_LIBS \
  LIBXUL_LIBRARY \
  MOCHITEST_A11Y_FILES \
  MOCHITEST_BROWSER_FILES \
  MOCHITEST_BROWSER_FILES_PARTS \
  MOCHITEST_CHROME_FILES \
  MOCHITEST_FILES \
  MOCHITEST_FILES_PARTS \
  MOCHITEST_METRO_FILES \
  MOCHITEST_ROBOCOP_FILES \
  SHORT_LIBNAME \
  TESTING_JS_MODULES \
  TESTING_JS_MODULE_DIR \
  $(NULL)

# Freeze the values specified by moz.build to catch them if they fail.

$(foreach var,$(_MOZBUILD_EXTERNAL_VARIABLES) $(_DEPRECATED_VARIABLES),$(eval $(var)_FROZEN := '$($(var))'))

TIERS := export $(if $(COMPILE_ENVIRONMENT),compile )misc libs tools
endif

# These defines are used to support the twin-topsrcdir model for comm-central.
ifdef MOZILLA_SRCDIR
  MOZILLA_DIR = $(MOZILLA_SRCDIR)
else
  MOZILLA_DIR = $(topsrcdir)
endif
