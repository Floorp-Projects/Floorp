# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This .mk file is included by all the branding Makefiles. It defines
# variables that are common to all.

ifndef top_srcdir
$(error Must define top_srcdir before including this file)
endif

ifndef srcdir
$(error Must define srcdir before including this file)
endif

DIRS := content locales
PREF_JS_EXPORTS := $(srcdir)/pref/firefox-branding.js

# These are the lists of branding files per platform. These are shared
# across all branding setups.
#
# If you add files to one branding config, you should define the
# corresponding variable in the respective Makefile and then include this
# file.
#
# If you remove a file from one branding config, that's not currently
# supported. You should add support for that in this file somehow.
# Alternatively, you can just reimplement the logic in this file.

windows_files += \
  firefox.ico \
  document.ico \
  branding.nsi \
  wizHeader.bmp \
  wizHeaderRTL.bmp \
  wizWatermark.bmp \
  newwindow.ico \
  newtab.ico \
  pbmode.ico \
  $(NULL)

osx_files += \
  background.png \
  firefox.icns \
  disk.icns \
  document.icns \
  dsstore \
  $(NULL)

linux_files += \
  default16.png \
  default32.png \
  default48.png \
  mozicon128.png \
  $(NULL)

os2_files += \
  firefox-os2.ico \
  document-os2.ico \
  $(NULL)

BRANDING_DEST := $(DIST)/branding

ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
BRANDING_FILES := $(windows_files)
endif
ifeq ($(MOZ_WIDGET_TOOLKIT),cocoa)
BRANDING_FILES := $(osx_files)
endif
ifeq ($(MOZ_WIDGET_TOOLKIT),gtk2)
BRANDING_FILES := $(linux_files)
endif
ifeq ($(OS_ARCH),OS2)
BRANDING_FILES := $(os2_files)
endif

BRANDING_FILES := $(addprefix $(srcdir)/,$(BRANDING_FILES))

ifneq ($(BRANDING_FILES),)
INSTALL_TARGETS += BRANDING
endif

include $(topsrcdir)/config/rules.mk
