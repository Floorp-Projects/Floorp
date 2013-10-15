includedir := $(includedir)/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
idldir = $(datadir)/idl/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
installdir = $(libdir)/$(MOZ_APP_NAME)-$(MOZ_APP_VERSION)
sdkdir = $(libdir)/$(MOZ_APP_NAME)-devel-$(MOZ_APP_VERSION)
DIST = $(DEPTH)/dist

# We do magic with OBJ_SUFFIX in config.mk, the following ensures we don't
# manually use it before config.mk inclusion
_OBJ_SUFFIX := $(OBJ_SUFFIX)
OBJ_SUFFIX = $(error config/config.mk needs to be included before using OBJ_SUFFIX)

ifeq ($(HOST_OS_ARCH),WINNT)
# We only support building with pymake or a specially built gnu make.
ifndef .PYMAKE
ifeq (,$(filter mozmake%,$(notdir $(MAKE))))
$(error Only building with pymake or mozmake is supported.)
endif
endif
ifeq (a,$(firstword a$(subst /, ,$(srcdir))))
$(error MSYS-style srcdir are not supported for Windows builds.)
endif
endif # WINNT

ifdef .PYMAKE
include_deps = $(eval -includedeps $(1))
else
include_deps = $(eval -include $(1))
endif
