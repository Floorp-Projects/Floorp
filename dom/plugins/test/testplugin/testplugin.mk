#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
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
