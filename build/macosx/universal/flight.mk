# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

topsrcdir = $(TOPSRCDIR)
include $(OBJDIR)/config/autoconf.mk

DIST = $(OBJDIR)/dist

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
APPNAME = $(MOZ_MACBUNDLE_NAME)
INSTALLER_DIR = $(MOZ_BUILD_APP)/installer
ifeq ($(MOZ_BUILD_APP),xulrunner) # {
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
ifdef ENABLE_TESTS
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
endif
