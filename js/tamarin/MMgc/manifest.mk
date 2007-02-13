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
# The Original Code is [Open Source Virtual Machine].
#
# The Initial Developer of the Original Code is
# Adobe System Incorporated.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
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

STATIC_LIBRARIES += MMgc
MMgc_BUILD_ALL = 1

MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/MMgc.cpp \
  $(curdir)/FixedAlloc.cpp \
  $(curdir)/FixedMalloc.cpp \
  $(curdir)/GC.cpp \
  $(curdir)/GCAlloc.cpp \
  $(curdir)/GCHashtable.cpp \
  $(curdir)/GCHeap.cpp \
  $(curdir)/GCLargeAlloc.cpp \
  $(curdir)/GCMemoryProfiler.cpp \
  $(curdir)/GCObject.cpp \
  $(curdir)/GCTests.cpp \
  $(NULL)

ifeq (windows,$(TARGET_OS))
MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/GCAllocObjectWin.cpp \
  $(curdir)/GCDebugWin.cpp \
  $(curdir)/GCHeapWin.cpp \
  $(NULL)
endif

ifeq (darwin,$(TARGET_OS))
MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/GCAllocObjectMac.cpp \
  $(curdir)/GCDebugMac.cpp \
  $(curdir)/GCHeapMac.cpp \
  $(NULL)
endif

ifeq (linux,$(TARGET_OS))
MMgc_CXXSRCS := $(MMgc_CXXSRCS) \
  $(curdir)/GCAllocObjectUnix.cpp \
  $(curdir)/GCDebugUnix.cpp \
  $(curdir)/GCHeapUnix.cpp \
  $(NULL)
endif

$(curdir)/GCDebugMac.$(OBJ_SUFFIX): CXXFLAGS += -Wno-deprecated-declarations
