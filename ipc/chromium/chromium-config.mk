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

LOCAL_INCLUDES += \
  -I$(topsrcdir)/ipc/chromium/src \
  -I$(topsrcdir)/ipc/glue \
  -I$(DEPTH)/ipc/ipdl/_ipdlheaders \
  $(NULL)

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
OS_POSIX = 1
DEFINES += -DOS_POSIX=1

ifeq ($(OS_ARCH),Darwin) # {

OS_MACOSX = 1
DEFINES += \
  -DOS_MACOSX=1 \
  $(NULL)

else # } {
ifeq ($(OS_ARCH),DragonFly) # {

OS_DRAGONFLY = 1
OS_BSD = 1
OS_LIBS += $(call EXPAND_LIBNAME,kvm)
DEFINES += \
  -DOS_DRAGONFLY=1 \
  -DOS_BSD=1 \
  $(NULL)

else # } {
ifneq (,$(filter $(OS_ARCH),FreeBSD GNU_kFreeBSD)) # {

OS_FREEBSD = 1
OS_BSD = 1
ifneq ($(OS_ARCH),GNU_kFreeBSD)
OS_LIBS += $(call EXPAND_LIBNAME,kvm)
endif
DEFINES += \
  -DOS_FREEBSD=1 \
  -DOS_BSD=1 \
  $(NULL)

else # } {
ifeq ($(OS_ARCH),NetBSD) # {

OS_NETBSD = 1
OS_BSD = 1
OS_LIBS += $(call EXPAND_LIBNAME,kvm)
DEFINES += \
  -DOS_NETBSD=1 \
  -DOS_BSD=1 \
  $(NULL)

else # } {
ifeq ($(OS_ARCH),OpenBSD) # {

OS_OPENBSD = 1
OS_BSD = 1
OS_LIBS += $(call EXPAND_LIBNAME,kvm)
DEFINES += \
  -DOS_OPENBSD=1 \
  -DOS_BSD=1 \
  $(NULL)

else # } {

OS_LINUX = 1
DEFINES += \
  -DOS_LINUX=1 \
  $(NULL)

endif # }
endif # }
endif # }
endif # }
endif # }
endif # }

