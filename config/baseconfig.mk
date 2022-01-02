# This file is normally included by autoconf.mk, but it is also used
# directly in python/mozbuild/mozbuild/base.py for gmake validation.
# We thus use INCLUDED_AUTOCONF_MK to enable/disable some parts depending
# whether a normal build is happening or whether the check is running.
installdir = $(libdir)/$(MOZ_APP_NAME)
ifeq (.,$(DEPTH))
DIST = dist
else
DIST = $(DEPTH)/dist
endif
ABS_DIST = $(topobjdir)/dist

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

ifeq ($(MOZ_BUILD_APP),tools/rusttests)
# Rusttest tiers aren't a subset of regular ALL_TIERS, so define them separately
ALL_TIERS := pre-export export rusttests
else
# All possible tiers
ALL_TIERS := artifact win32-artifact android-fat-aar-artifact pre-export export pre-compile rust compile misc libs android-stage-package android-archive-geckoview tools check
endif

# All tiers that may be used manually via `mach build $tier`
RUNNABLE_TIERS := $(ALL_TIERS)
ifndef MOZ_ARTIFACT_BUILDS
RUNNABLE_TIERS := $(filter-out artifact,$(RUNNABLE_TIERS))
endif
ifndef MOZ_EME_WIN32_ARTIFACT
RUNNABLE_TIERS := $(filter-out win32-artifact,$(RUNNABLE_TIERS))
endif
ifndef MOZ_ANDROID_FAT_AAR_ARCHITECTURES
RUNNABLE_TIERS := $(filter-out android-fat-aar-artifact,$(RUNNABLE_TIERS))
endif
ifneq ($(MOZ_BUILD_APP),mobile/android)
RUNNABLE_TIERS := $(filter-out android-stage-package,$(RUNNABLE_TIERS))
RUNNABLE_TIERS := $(filter-out android-archive-geckoview,$(RUNNABLE_TIERS))
endif

# All tiers that run automatically on `mach build`
TIERS := $(filter-out pre-compile check,$(RUNNABLE_TIERS))
ifndef COMPILE_ENVIRONMENT
TIERS := $(filter-out rust compile,$(TIERS))
endif
ifndef MOZ_RUST_TIER
TIERS := $(filter-out rust,$(TIERS))
endif

endif

# These defines are used to support the twin-topsrcdir model for comm-central.
ifdef MOZILLA_SRCDIR
  MOZILLA_DIR = $(MOZILLA_SRCDIR)
else
  MOZILLA_DIR = $(topsrcdir)
endif
