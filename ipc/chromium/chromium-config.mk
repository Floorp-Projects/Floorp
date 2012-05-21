# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_CONFIG_MK
$(error Must include config.mk before this file.)
endif

ifdef CHROMIUM_CONFIG_INCLUDED
$(error Must not include chromium-config.mk twice.)
endif

CHROMIUM_CONFIG_INCLUDED = 1

EXTRA_DEPS += $(topsrcdir)/ipc/chromium/chromium-config.mk

DEFINES += \
  -DEXCLUDE_SKIA_DEPENDENCIES \
  $(NULL)

LOCAL_INCLUDES += \
  -I$(topsrcdir)/ipc/chromium/src \
  -I$(topsrcdir)/ipc/glue \
  -I$(DEPTH)/ipc/ipdl/_ipdlheaders \
  $(NULL)

ifeq ($(OS_ARCH),Darwin) # {

OS_MACOSX = 1
OS_POSIX = 1

DEFINES += \
  -DOS_MACOSX=1 \
  -DOS_POSIX=1 \
  $(NULL)

else # } {
ifeq ($(OS_ARCH),WINNT) # {
OS_LIBS += $(call EXPAND_LIBNAME,psapi shell32 dbghelp)

OS_WIN = 1

DEFINES += \
  -DUNICODE \
  -D_UNICODE \
  -DNOMINMAX \
  -D_CRT_RAND_S \
  -DCERT_CHAIN_PARA_HAS_EXTRA_FIELDS \
  -D_SECURE_ATL \
  -DCHROMIUM_BUILD \
  -DU_STATIC_IMPLEMENTATION \
  -DOS_WIN=1 \
  -DWIN32 \
  -D_WIN32 \
  -D_WINDOWS \
  -DWIN32_LEAN_AND_MEAN \
  $(NULL)

ifdef _MSC_VER
DEFINES += -DCOMPILER_MSVC
endif

else # } {

OS_LINUX = 1
OS_POSIX = 1

DEFINES += \
  -DOS_LINUX=1 \
  -DOS_POSIX=1 \
  $(NULL)

# NB: to stop gcc warnings about exporting template instantiation
OS_CXXFLAGS := $(filter-out -pedantic,$(OS_CXXFLAGS))

endif # }
endif # }

