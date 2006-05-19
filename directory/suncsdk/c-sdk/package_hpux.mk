# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Sun LDAP C SDK.
# 
# The Initial Developer of the Original Code is Sun Microsystems, 
# Inc. Portions created by Sun Microsystems, Inc are Copyright 
# (C) 2005 Sun Microsystems, Inc.  All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

# This file is to create HP-UX depots,
# has 3 targets and packages individually,
# creates the packages in (MM.DD)/(OBJDIR)/packages directory.

DEPTH	= ../..
NSPR_TREE = .
MOD_DEPTH = .

include $(NSPR_TREE)/config/rules.mk
include build.mk

MMDD	= $(shell date +%m.%d)

SRC_DIR = ../../dist/$(MMDD)/$(OS_ARCH)$(OS_RELEASE)$(OBJDIR_TAG).OBJ
SRC_DIR_64= ../../dist/$(MMDD)/$(OS_ARCH)$(OS_RELEASE)_64$(OBJDIR_TAG).OBJ

REVISION = $(shell date +%Y.%m.%d.%H.%M)
PKG_DIR = ../pkg/hpux
DEPOT_DIR=$(SRC_DIR)/packages
swpackage_args=-x follow_symlinks=true \
		-x compression_type=gzip \
		-x compress_cmd=/usr/contrib/bin/gzip \
		-x uncompress_files=false \
		-x compress_files=true \
		-x reinstall_files=true \
		-x package_in_place=false \
		-x target_type=directory \
		-x write_remote_files=true \
		-x run_as_superuser=false 

ifndef DEPOT_VERSION
DEPOT_VERSION= $(shell expr "scale=2; $(VENDOR_VERSION)/100" | bc)
endif

all:: sun-ldapcsdk-libs sun-ldapcsdk-tools sun-ldapcsdk-dev


sun-ldapcsdk-libs:
	sed -e 's/#DEPOT_VERSION#/$(DEPOT_VERSION)/g' -e 's/#REVISION#/$(REVISION)/g' -e 's:{SRC_DIR}:$(SRC_DIR):g' -e 's:{SRC_DIR_64}:${SRC_DIR_64}:g' $(PKG_DIR)/sun-ldapcsdk-libs/sun-ldapcsdk-libs.psf.template > $(PKG_DIR)/sun-ldapcsdk-libs/sun-ldapcsdk-libs.psf ; 
	/usr/sbin/swpackage $(swpackage_args) -s $(PKG_DIR)/sun-ldapcsdk-libs/sun-ldapcsdk-libs.psf @$(DEPOT_DIR)/sun-ldapcsdk-libs 
ifdef BUILD_SHIP
	@echo "Copying depot to $(BUILD_SHIP)/$(OBJDIR_NAME) directory"
	cp -R $(DEPOT_DIR)/sun-ldapcsdk-libs $(BUILD_SHIP)/$(OBJDIR_NAME)/
endif

sun-ldapcsdk-tools:
	sed -e 's/#DEPOT_VERSION#/$(DEPOT_VERSION)/g' -e 's/#REVISION#/$(REVISION)/g' -e 's:{SRC_DIR}:$(SRC_DIR):g' -e 's:{SRC_DIR_64}:${SRC_DIR_64}:g' $(PKG_DIR)/sun-ldapcsdk-tools/sun-ldapcsdk-tools.psf.template > $(PKG_DIR)/sun-ldapcsdk-tools/sun-ldapcsdk-tools.psf ; \
	/usr/sbin/swpackage $(swpackage_args) -s $(PKG_DIR)/sun-ldapcsdk-tools/sun-ldapcsdk-tools.psf @$(DEPOT_DIR)/sun-ldapcsdk-tools
ifdef BUILD_SHIP
	@echo "Copying depot to $(BUILD_SHIP)/$(OBJDIR_NAME) directory"
	cp -R $(DEPOT_DIR)/sun-ldapcsdk-tools $(BUILD_SHIP)/$(OBJDIR_NAME)/
endif

sun-ldapcsdk-dev:
	sed -e 's/#DEPOT_VERSION#/$(DEPOT_VERSION)/g' -e 's/#REVISION#/$(REVISION)/g' -e 's:{SRC_DIR}:$(SRC_DIR):g' -e 's:{SRC_DIR_64}:${SRC_DIR_64}:g' $(PKG_DIR)/sun-ldapcsdk-dev/sun-ldapcsdk-dev.psf.template > $(PKG_DIR)/sun-ldapcsdk-dev/sun-ldapcsdk-dev.psf ;\
	/usr/sbin/swpackage $(swpackage_args) -s $(PKG_DIR)/sun-ldapcsdk-dev/sun-ldapcsdk-dev.psf @$(DEPOT_DIR)/sun-ldapcsdk-dev
ifdef BUILD_SHIP
	@echo "Copying depot to $(BUILD_SHIP)/$(OBJDIR_NAME) directory"
	cp -R $(DEPOT_DIR)/sun-ldapcsdk-dev $(BUILD_SHIP)/$(OBJDIR_NAME)/
endif

clean:: FORCE
	@echo "Cleaning up old depots"
	$(RM) -rf $(DEPOT_DIR)

FORCE::
