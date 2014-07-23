#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq ($(MOZ_WIDGET_TOOLKIT),qt)
include $(topsrcdir)/config/config.mk
CXXFLAGS        += $(MOZ_QT_CFLAGS)
CFLAGS          += $(MOZ_QT_CFLAGS)
OS_LIBS += \
                $(MOZ_QT_LIBS) \
                $(XLDFLAGS) \
                $(XLIBS)
endif

ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
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

ifneq (,$(filter gtk2 gtk3,$(MOZ_WIDGET_TOOLKIT)))
CXXFLAGS        += $(MOZ_GTK2_CFLAGS)
CFLAGS          += $(MOZ_GTK2_CFLAGS)
OS_LIBS += $(MOZ_GTK2_LIBS) $(XLDFLAGS) $(XLIBS) $(XEXT_LIBS)
endif
