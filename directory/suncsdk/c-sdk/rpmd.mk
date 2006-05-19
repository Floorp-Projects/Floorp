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

DEPTH	= ../..
NSPR_TREE = .
MOD_DEPTH = .

include $(NSPR_TREE)/config/rules.mk
include build.mk

MMDD	= $(shell date +%m.%d)
INSTDIR = $(shell cd ../../dist/$(MMDD)/$(OBJDIR_NAME); pwd)
ABS_DIR = $(shell cd ../../dist/$(OBJDIR_NAME); pwd)

# linux rpm packaging variables

RPM_ROOTDIR=$(ABS_DIR)/rpm/tree
RPM_GENDIR=$(ABS_DIR)/rpm/rpmgen
RPM_PREFIX_PKGNAME=sun
RPM_PREFIX_USR=/usr
RPM_PREFIX=/opt/${RPM_PREFIX_PKGNAME}
RPM_PKGNAME=ldapcsdk-dev
ifndef RPM_PKGVERSION
RPM_PKGVERSION=6.00
endif
ifndef RPM_RELEASE
RPM_RELEASE=4
endif
RPM_DIR=$(RPM_ROOTDIR)$(RPM_PREFIX)
RPM_DIR_USR=$(RPM_ROOTDIR)$(RPM_PREFIX_USR)
RPM_NAME=$(RPM_PREFIX_PKGNAME)-$(RPM_PKGNAME)-$(RPM_PKGVERSION)-$(RPM_RELEASE).i386.rpm

#patch specific

RPM_PATCH_ROOTDIR=$(ABS_DIR)/rpm/patchtree
RPM_PATCH_GENDIR=$(ABS_DIR)/rpm/rpmpatchgen
RPM_PATCH_NO=118353
RPM_PATCH_REV=04

RPM_PATCH_ID = $(RPM_PATCH_NO)-$(RPM_PATCH_REV)
#RPM_PATCH_RELEASE=1
RPM_PATCH_DIR=$(RPM_PATCH_ROOTDIR)$(RPM_PREFIX)

DATE=$(shell date +%b/%d/%y)
LASTMOD_D=$(shell date +"%A, %B %d, %Y")
#RPM_PATCH_NAME=$(RPM_PREFIX_PKGNAME)-$(RPM_PKGNAME)-$(RPM_PKGVERSION)-$(RPM_RELEASE).$(RPM_PATCH_RELEASE).i386.rpm
RPM_PATCH_NAME=$(RPM_PREFIX_PKGNAME)-$(RPM_PKGNAME)-$(RPM_PKGVERSION)-$(RPM_RELEASE).i386.rpm

#end patch specific

ifndef NO_CHECKS
makeRpm: createRpm checks
else
makeRpm: createRpm
endif


createRpm:
	@echo "Creating RPM package"
	rm -rf $(RPM_GENDIR)
	rm -f $(INSTDIR)/$(RPM_NAME)
	rm -rf $(RPM_DIR)
	mkdir -p $(RPM_DIR)/private/include/ldap
	mkdir -p $(RPM_GENDIR)/BUILD
	mkdir -p $(RPM_GENDIR)/RPMS
	cp $(INSTDIR)/include/disptmpl.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/lber.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap-deprecated.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap-extension.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap-platform.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap-standard.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap-to-be-deprecated.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldap_ssl.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/ldappr.h $(RPM_DIR)/private/include/ldap
	cp $(INSTDIR)/include/srchpref.h $(RPM_DIR)/private/include/ldap
	rm -rf $(RPM_DIR_USR)
	mkdir -p $(RPM_DIR_USR)/share/ldapcsdk/etc
	mkdir -p $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/libraries/libldap/ldapfilter.conf $(RPM_DIR_USR)/share/ldapcsdk/etc
	cp ./ldap/libraries/libldap/ldapfriendly $(RPM_DIR_USR)/share/ldapcsdk/etc
	cp ./ldap/libraries/libldap/ldapsearchprefs.conf $(RPM_DIR_USR)/share/ldapcsdk/etc
	cp ./ldap/libraries/libldap/ldaptemplates.conf $(RPM_DIR_USR)/share/ldapcsdk/etc
	cp ./ldap/examples/README $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/Makefile $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/xmplflt.conf $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/examples.h $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/add.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/asearch.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/authzid.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/compare.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/crtfilt.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/csearch.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/del.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/effright.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/getattrs.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/getfilt.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/modattrs.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/modrdn.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/nsprio.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/ppolicy.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/psearch.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/pwdextop.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/pwdpolicy.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/rdentry.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/realattr.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/search.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/srvrsort.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/ssearch.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/ssnoauth.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/starttls.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/userstatus.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/virtattr.c $(RPM_DIR_USR)/share/ldapcsdk/examples
	cp ./ldap/examples/whoami.c $(RPM_DIR_USR)/share/ldapcsdk/examples

	sed -e 's|%RPM_ROOTDIR%|$(RPM_ROOTDIR)|g' \\
	    -e 's|%RPM_GENDIR%|$(RPM_GENDIR)|g' \\
	    -e 's|%RPM_PREFIX%|$(RPM_PREFIX)|g' \\
		-e 's|%RPM_PREFIX_USR%|$(RPM_PREFIX_USR)|g' \\
	    -e 's|%RPM_PREFIX_PKGNAME%|$(RPM_PREFIX_PKGNAME)|g' \\
	    -e 's|%RPM_PKGNAME%|$(RPM_PKGNAME)|g' \\
	    -e 's|%RPM_RELEASE%|$(RPM_RELEASE)|g' \\
	    -e 's|%RPM_PKGVERSION%|$(RPM_PKGVERSION)|g' \\
	        ldapsdkd.spec.template > $(RPM_GENDIR)/ldapsdkd.spec
	rpmbuild -bb --target=i386 $(RPM_GENDIR)/ldapsdkd.spec
	cp $(RPM_GENDIR)/RPMS/i386/$(RPM_NAME) $(INSTDIR)
ifdef BUILD_SHIP
	@echo "Copying rpm package to $(BUILD_SHIP) directory"
	cp $(RPM_GENDIR)/RPMS/i386/$(RPM_NAME) $(BUILD_SHIP)/$(OBJDIR_NAME)
endif   
	rm -rf $(RPM_GENDIR)
	rm -rf $(RPM_DIR)

checks:
#	if [ -f $(INSTDIR)/$(RPM_NAME) ] ; then \\
#		rsh $(INTEGRATE_HOST) "$(INTEGRATE_CMD) -n -l ldap_sdk_devel -r s9_lorion3 -R $(INSTDIR)/$(RPM_NAME)" ; \\
#	else  \\
#		echo $(INSTDIR)/$(RPM_NAME) not found ; \\
#	fi


#patch specific

ifndef NO_CHECKS
makePatchRpm: patchRpm patchchecks
else
makePatchRpm: patchRpm 
endif

patchRpm:
	@echo "Creating RPM patch"
	rm -rf $(RPM_PATCH_GENDIR)
	rm -rf $(INSTDIR)/$(RPM_PATCH_ID)
	rm -rf $(RPM_PATCH_DIR)
	mkdir -p $(RPM_PATCH_DIR)/private/lib
	mkdir -p $(RPM_PATCH_GENDIR)/BUILD
	mkdir -p $(RPM_PATCH_GENDIR)/RPMS
	cp $(INSTDIR)/lib/libldap50.so $(RPM_PATCH_DIR)/private/lib
	cp $(INSTDIR)/lib/libssldap50.so $(RPM_PATCH_DIR)/private/lib
	cp $(INSTDIR)/lib/libprldap50.so $(RPM_PATCH_DIR)/private/lib
	sed -e 's|%RPM_ROOTDIR%|$(RPM_PATCH_ROOTDIR)|g' \\
	    -e 's|%RPM_GENDIR%|$(RPM_PATCH_GENDIR)|g' \\
	    -e 's|%RPM_PREFIX%|$(RPM_PREFIX)|g' \\
	    -e 's|%RPM_PREFIX_PKGNAME%|$(RPM_PREFIX_PKGNAME)|g' \\
	    -e 's|%RPM_PKGNAME%|$(RPM_PKGNAME)|g' \\
	    -e 's|%RPM_RELEASE%|$(RPM_RELEASE)|g' \\
	    -e 's|%RPM_PKGVERSION%|$(RPM_PKGVERSION)|g' \\
		ldapsdk.spec.template > $(RPM_PATCH_GENDIR)/ldapsdk_patch.spec
	rpmbuild -bb --target=i386 $(RPM_PATCH_GENDIR)/ldapsdk_patch.spec
	mkdir -p $(INSTDIR)/$(RPM_PATCH_ID)
	sed -e 's|RPM_PATCHID|$(RPM_PATCH_ID)|g' \\
	    -e 's|RPM_PATCH_REV|$(RPM_PATCH_REV)|g' \\
	    -e 's|DATE|$(DATE)|g' \\
		-e 's|LASTMOD_D|$(LASTMOD_D)|g' \\
	    -e 's|RPM_PATCHNAME|$(RPM_PATCH_NAME)|g' \\
		README-linux > $(RPM_PATCH_GENDIR)/README.$(RPM_PATCH_ID)
	cp $(RPM_PATCH_GENDIR)/RPMS/i386/*.rpm $(INSTDIR)/$(RPM_PATCH_ID)
	cp $(RPM_PATCH_GENDIR)/README.$(RPM_PATCH_ID) $(INSTDIR)/$(RPM_PATCH_ID)
#	@echo "Checking patch..."
#	@echo "running: $(CHECKPATCH) -f $(INSTDIR)/$(RPM_PATCH_ID)"
#	$(CHECKPATCH) -f $(INSTDIR)/$(RPM_PATCH_ID) 2>&1
ifdef BUILD_SHIP
	@echo "Copying rpm patch to $(BUILD_SHIP) directory"
	mkdir -p $(BUILD_SHIP)/$(OBJDIR_NAME)/$(RPM_PATCH_ID)
	cp $(RPM_PATCH_GENDIR)/RPMS/i386/*.rpm $(BUILD_SHIP)/$(OBJDIR_NAME)/$(RPM_PATCH_ID)
	cp $(RPM_PATCH_GENDIR)/README.$(RPM_PATCH_ID) $(BUILD_SHIP)/$(OBJDIR_NAME)/$(RPM_PATCH_ID)
	
endif

patchchecks:
