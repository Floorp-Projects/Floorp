# This file is normally included by autoconf.mk, but it is also used
# directly in python/mozbuild/mozbuild/base.py for gmake validation.
# We thus use INCLUDED_AUTOCONF_MK to enable/disable some parts depending
# whether a normal build is happening or whether the check is running.
includedir := $(includedir)/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
idldir = $(datadir)/idl/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
installdir = $(libdir)/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
sdkdir = $(libdir)/$(MOZ_APP_NAME)-devel-$(MOZ_APP_VERSION)
ifeq (.,$(DEPTH))
DIST = dist
else
DIST = $(DEPTH)/dist
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

ifndef INCLUDED_AUTOCONF_MK
default::
else
TIERS := export $(if $(COMPILE_ENVIRONMENT),compile )misc libs tools
endif

# These defines are used to support the twin-topsrcdir model for comm-central.
ifdef MOZILLA_SRCDIR
  MOZILLA_DIR = $(MOZILLA_SRCDIR)
else
  MOZILLA_DIR = $(topsrcdir)
endif
