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
# We only support building with pymake or a non-msys gnu make version
# strictly above 4.0.
ifndef .PYMAKE
ifeq (a,$(firstword a$(subst /, ,$(abspath .))))
$(error MSYS make is not supported)
endif
# 4.0- happens to be greater than 4.0, lower than the mozmake version,
# and lower than 4.0.1 or 4.1, whatever next version of gnu make will
# be released.
ifneq (4.0-,$(firstword $(sort 4.0- $(MAKE_VERSION))))
$(error Make version too old. Only versions strictly greater than 4.0 are supported.)
endif
endif
ifeq (a,$(firstword a$(subst /, ,$(srcdir))))
$(error MSYS-style srcdir are not supported for Windows builds.)
endif
endif # WINNT

ifdef .PYMAKE
include_deps = $(eval $(if $(2),,-)includedeps $(1))
else
include_deps = $(eval $(if $(2),,-)include $(1))
endif
