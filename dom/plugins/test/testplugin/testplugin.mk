#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

FORCE_SHARED_LIB = 1

# Don't use STL wrappers; nptest isn't Gecko code
STL_FLAGS =

# must link statically with the CRT; nptest isn't Gecko code
USE_STATIC_LIBS = 1

# Need to custom install plugins
NO_DIST_INSTALL	= 1
NO_INSTALL = 1

VPATH += $(topsrcdir)/build

CPPSRCS   =  \
  $(RELATIVE_PATH)/nptest.cpp \
  $(RELATIVE_PATH)/nptest_utils.cpp \
  nptest_name.cpp \
  $(NULL)

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
CMMSRCS = $(RELATIVE_PATH)/nptest_macosx.mm
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),gtk2)
CPPSRCS += $(RELATIVE_PATH)/nptest_gtk2.cpp
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),android)
CPPSRCS += $(RELATIVE_PATH)/nptest_droid.cpp
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),os2)
CPPSRCS += $(RELATIVE_PATH)/nptest_os2.cpp
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),qt)
CPPSRCS += $(RELATIVE_PATH)/nptest_qt.cpp
include $(topsrcdir)/config/config.mk
CXXFLAGS        += $(MOZ_QT_CFLAGS)
CFLAGS          += $(MOZ_QT_CFLAGS)
EXTRA_DSO_LDOPTS = \
                $(MOZ_QT_LIBS) \
                $(XLDFLAGS) \
                $(XLIBS)
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
CPPSRCS  += $(RELATIVE_PATH)/nptest_windows.cpp
RCFILE    = nptest.rc
RESFILE   = nptest.res
DEFFILE   = $(win_srcdir)/nptest.def
OS_LIBS  += $(call EXPAND_LIBNAME,msimg32)

# Windows opt builds without PGO break nptest.dll
MOZ_OPTIMIZE=
endif

TEST_PLUGIN_FILES = $(SHARED_LIBRARY)

ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
MAC_PLIST_FILES += $(srcdir)/Info.plist
MAC_PLIST_DEST   = $(DIST)/plugins/$(COCOA_NAME).plugin/Contents
TEST_PLUGIN_DEST = $(DIST)/plugins/$(COCOA_NAME).plugin/Contents/MacOS
INSTALL_TARGETS += \
	TEST_PLUGIN \
	MAC_PLIST \
	$(NULL)
else
TEST_PLUGIN_DEST = $(DIST)/plugins
INSTALL_TARGETS += TEST_PLUGIN
endif

include $(topsrcdir)/config/rules.mk

ifndef __LP64__
ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
EXTRA_DSO_LDOPTS += -framework Carbon
endif
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),gtk2)
CXXFLAGS        += $(MOZ_GTK2_CFLAGS)
CFLAGS          += $(MOZ_GTK2_CFLAGS)
EXTRA_DSO_LDOPTS += $(MOZ_GTK2_LIBS) $(XLDFLAGS) $(XLIBS) $(XEXT_LIBS)
endif
