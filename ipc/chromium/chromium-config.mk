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
# The Original Code is the Mozilla platform.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation <http://www.mozilla.org/>.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

ifndef INCLUDED_CONFIG_MK
$(error Must include config.mk before this file.)
endif

ifdef CHROMIUM_CONFIG_INCLUDED
$(error Must not include chromium-config.mk twice.)
endif

CHROMIUM_CONFIG_INCLUDED = 1

EXTRA_DEPS += $(topsrcdir)/ipc/chromium/chromium-config.mk

ifdef MOZ_IPC # {

DEFINES += \
  -DEXCLUDE_SKIA_DEPENDENCIES \
  -DCHROMIUM_MOZILLA_BUILD \
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
  -DCOMPILER_MSVC \
  -DOS_WIN=1 \
  -DWIN32 \
  -D_WIN32 \
  -D_WINDOWS \
  -DWIN32_LEAN_AND_MEAN \
  $(NULL)

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

endif # }