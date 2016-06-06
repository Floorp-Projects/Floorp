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
DEPTH = $(OBJDIR)
include $(OBJDIR)/config/autoconf.mk

DIST = $(OBJDIR)/dist

postflight_all:
	mkdir -p $(DIST_UNI)/$(MOZ_PKG_APPNAME)
	rm -f $(DIST_ARCH_2)/universal
	ln -s $(abspath $(DIST_UNI)) $(DIST_ARCH_2)/universal
# Stage a package for buildsymbols to be happy. Doing so in OBJDIR_ARCH_1
# actually does a universal staging with both OBJDIR_ARCH_1 and OBJDIR_ARCH_2.
	$(MAKE) -C $(OBJDIR_ARCH_1)/$(MOZ_BUILD_APP)/installer \
	   PKG_SKIP_STRIP=1 stage-package
