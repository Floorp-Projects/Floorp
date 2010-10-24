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
# The Original Code is the Mozilla Mac OS X Universal Binary Packaging System
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mark Mentovai <mark@moxienet.com> (Original Author)
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

# BE CAREFUL!  This makefile handles a postflight_all rule for a
# multi-project build, so DON'T rely on anything that might differ between
# the two OBJDIRs.

ifndef OBJDIR
OBJDIR_ARCH_1 = $(MOZ_OBJDIR)/$(firstword $(MOZ_BUILD_PROJECTS))
OBJDIR_ARCH_2 = $(MOZ_OBJDIR)/$(word 2,$(MOZ_BUILD_PROJECTS))
DIST_ARCH_1 = $(OBJDIR_ARCH_1)/dist
DIST_ARCH_2 = $(OBJDIR_ARCH_2)/dist
DIST_UNI = $(DIST_ARCH_1)/universal
OBJDIR = $(OBJDIR_ARCH_1)
endif

include $(OBJDIR)/config/autoconf.mk

DIST = $(OBJDIR)/dist

ifdef MOZ_DEBUG
DBGTAG = Debug
else
DBGTAG =
endif

ifdef LIBXUL_SDK # {
APP_CONTENTS = Contents/Frameworks/XUL.framework
else # } {
APP_CONTENTS = Contents/MacOS
endif # } LIBXUL_SDK

ifeq ($(MOZ_BUILD_APP),camino) # {
INSTALLER_DIR = camino/installer
MOZ_PKG_APPNAME = camino
APPNAME = Camino.app
BUILDCONFIG_BASE = Contents/MacOS/chrome
else # } {
MOZ_PKG_APPNAME = $(MOZ_APP_NAME)
APPNAME = $(MOZ_APP_DISPLAYNAME)$(DBGTAG).app
INSTALLER_DIR = $(MOZ_BUILD_APP)/installer
ifeq ($(MOZ_BUILD_APP),xulrunner) # {
INSTALLER_DIR = xulrunner/installer/mac
APPNAME = XUL.framework
APP_CONTENTS = Versions/Current
endif # } xulrunner
BUILDCONFIG_BASE = $(APP_CONTENTS)/chrome
endif # } !camino

ifeq ($(MOZ_CHROME_FILE_FORMAT),jar)
BUILDCONFIG = $(BUILDCONFIG_BASE)/toolkit.jar
FIX_MODE = jar
else
BUILDCONFIG = $(BUILDCONFIG_BASE)/toolkit/
FIX_MODE = file
endif

postflight_all:
# Build the universal package out of only the bits that would be released.
# Call the packager to set this up.  Set UNIVERSAL_BINARY= to avoid producing
# a universal binary too early, before the unified bits have been staged.
# Set SIGN_NSS= to skip shlibsign.
	$(MAKE) -C $(OBJDIR_ARCH_1)/$(INSTALLER_DIR) \
          UNIVERSAL_BINARY= SIGN_NSS= PKG_SKIP_STRIP=1 stage-package
	$(MAKE) -C $(OBJDIR_ARCH_2)/$(INSTALLER_DIR) \
          UNIVERSAL_BINARY= SIGN_NSS= PKG_SKIP_STRIP=1 stage-package
# Remove .chk files that may have been copied from the NSS build.  These will
# cause unify to warn or fail if present.  New .chk files that are
# appropriate for the merged libraries will be generated when the universal
# dmg is built.
	rm -f $(DIST_ARCH_1)/$(MOZ_PKG_APPNAME)/$(APPNAME)/$(APP_CONTENTS)/*.chk \
	      $(DIST_ARCH_2)/$(MOZ_PKG_APPNAME)/$(APPNAME)/$(APP_CONTENTS)/*.chk
# The only difference betewen the two trees now should be the
# about:buildconfig page.  Fix it up.
	$(TOPSRCDIR)/build/macosx/universal/fix-buildconfig $(FIX_MODE) \
	  $(DIST_ARCH_1)/$(MOZ_PKG_APPNAME)/$(APPNAME)/$(BUILDCONFIG) \
	  $(DIST_ARCH_2)/$(MOZ_PKG_APPNAME)/$(APPNAME)/$(BUILDCONFIG)
	mkdir -p $(DIST_UNI)/$(MOZ_PKG_APPNAME)
	rm -f $(DIST_ARCH_2)/universal
	ln -s $(DIST_UNI) $(DIST_ARCH_2)/universal
	rm -rf $(DIST_UNI)/$(MOZ_PKG_APPNAME)/$(APPNAME)
	$(TOPSRCDIR)/build/macosx/universal/unify \
          --unify-with-sort "\.manifest$$" \
          --unify-with-sort "components\.list$$" \
	  $(DIST_ARCH_1)/$(MOZ_PKG_APPNAME)/$(APPNAME) \
	  $(DIST_ARCH_2)/$(MOZ_PKG_APPNAME)/$(APPNAME) \
	  $(DIST_UNI)/$(MOZ_PKG_APPNAME)/$(APPNAME)
# A universal .dmg can now be produced by making in either architecture's
# INSTALLER_DIR.
# Now, repeat the process for the test package.
	$(MAKE) -C $(OBJDIR_ARCH_1) UNIVERSAL_BINARY= CHROME_JAR= package-tests
	$(MAKE) -C $(OBJDIR_ARCH_2) UNIVERSAL_BINARY= CHROME_JAR= package-tests
	rm -rf $(DIST_UNI)/test-package-stage
# automation.py differs because it hardcodes a path to
# dist/bin. It doesn't matter which one we use.
	if test -d $(DIST_ARCH_1)/test-package-stage -a                 \
                -d $(DIST_ARCH_2)/test-package-stage; then              \
           cp $(DIST_ARCH_1)/test-package-stage/mochitest/automation.py \
             $(DIST_ARCH_2)/test-package-stage/mochitest/;              \
           cp $(DIST_ARCH_1)/test-package-stage/reftest/automation.py   \
             $(DIST_ARCH_2)/test-package-stage/reftest/;                \
           $(TOPSRCDIR)/build/macosx/universal/unify                 \
             --unify-with-sort "all-test-dirs\.list$$"               \
             $(DIST_ARCH_1)/test-package-stage                          \
             $(DIST_ARCH_2)/test-package-stage                          \
             $(DIST_UNI)/test-package-stage; fi
