# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
#

LDAP_DEPTH	= .
NSPR_TREE	= ../nsprpub
NSS_TREE	= ../security
MOD_DEPTH	= ../nsprpub
CORECONFDIR	= ../coreconf
ALTCORECONFDIR	= ../../coreconf
DEPTH		= ..
CONFIGDIR	= c-sdk/config
MOD_DEPTH	= c-sdk

include		$(CONFIGDIR)/config.mk
include		c-sdk/build.mk

ifeq ($(COMPONENT_PULL_METHOD), FTP)
	COMP_PULL_ARG="USE_FTP=YES"
endif

all:	FORCE
	@echo
	@echo The following are build targets that you can choose from:
	@echo
	@echo "   gmake buildLdapClientSDK"
	@echo "   gmake clean"
	@echo
	@echo Allowed options:
	@echo 
	@echo "   BUILD_OPT=1		for optimized build"
	@echo "   USE_PTHREADS=1	for using pthreads"
	@echo "   USE_64=1		for building 64 bit versions"
	@echo "   BUILD_CLU=1		build command line utilities"
	@echo "   COMPS_FROM_OBJDIR=1	use components from ../dist/$(OBJDIR_NAME)"
	@echo "   VENDOR_NAME=string	SDK vendor name (default $(DEFAULT_VENDOR_NAME))"
	@echo "   VENDOR_VERSION=#	vendor specific version number * 100 (default $(DEFAULT_VENDOR_VERSION))"
	@echo 
	@echo Internal options
	@echo
	@echo "   gmake buildAndPkgLdapSDK"
	@echo "   gmake PullSDKComponents"
	@echo "   gmake pkgLdapSDK"
	@echo
	@echo "   HAVE_LIBNLS=1	for building/linking with LIBNLS"
	@echo "   HAVE_CCONF=1		for building with components"
	@echo "   HAVE_SVRCORE=1	for building with svrcore"
	@echo "   DONT_REPULL=1	for not repulling components"
	@echo "   PKG_PRIVATE_HDRS=0	do not ship private headers"
	@echo "   PKG_PRIVATE_LIBS=0	do not ship private libraries"
	@echo "   PKG_DEP_LIBS=0	do not ship dependent libraries"
ifneq ($(OS_ARCH), WINNT)
	@echo "   COMPONENT_PULL_METHOD=FTP     pull components using FTP"
endif

	@echo
	@echo "   For internal builds, coreconf must be installed"
	@echo "   in $(CORECONFDIR)"

buildLdapClientSDK export: PullSDKComponents
	@echo
	@echo ==== Starting LDAP Client SDK ==========
	@echo
ifneq ($(HAVE_CCONF), 1)
	@if [ -d $(NSPR_TREE)/ ]; then \
		echo "NSPR $(NSPR_VERSION) directory found"; \
	else \
		echo "No NSPR $(NSPR_VERSION) directory found"; \
	fi
	@if [  -d $(NSS_TREE)/ ]; then \
		echo "NSS $(NSS_VERSION) directory found"; \
	else \
		echo "No NSS $(NSS_VERSION) directory found"; \
	fi
else
	@if [ -d $(CORECONFDIR)/ ]; then \
		echo "coreconf $(CORECONFDIR) directory found"; \
	else \
		echo "No $(CORECONFDIR) found checking for alternate location"; \
		if [ -d $(ALTCORECONFDIR)/ ]; then \
			echo "found $(ALTCORECONFDIR)....copying to $(CORECONFDIR)/"; \
			cp -r $(ALTCORECONFDIR) $(CORECONFDIR); \
		else \
			echo "No coreconf directory found"; \
			exit 1; \
		fi \
	fi
	@if [ -d $(CORECONFDIR)/ ]; then \
		cd $(CORECONFDIR); \
		gmake; \
	else \
		echo "No $(CORECONFDIR) directory found"; \
	fi
endif
	@if [ -d $(CONFIGDIR)/ ]; then \
		cd $(CONFIGDIR); \
		gmake; \
	else \
		echo "No $(CONFIGDIR) directory found"; \
	fi
	@if [ -d $(LDAP_DEPTH)/c-sdk/ldap ]; then \
		cd $(LDAP_DEPTH)/c-sdk/ldap; \
		$(MAKE) -f Makefile.client $(MFLAGS) export; \
		$(MAKE) -f Makefile.client $(MFLAGS) install; \
	else \
		echo "No LDAP directory -- skipping"; \
		exit 0; \
	fi

pkgLdapSDK: FORCE
	@echo
	@echo ==== Starting LDAP Client SDK packaging ===
	@echo
	@if [ -d $(DIST_TREE)/ ]; then \
		cd $(LDAP_DEPTH)/c-sdk/; \
		$(MAKE) -f package.mk $(MFLAGS) all; \
	else \
		echo "No dist directory found -- can't package"; \
		exit 0; \
	fi

buildAndPkgLdapSDK: buildLdapClientSDK pkgLdapSDK

PullSDKComponents: FORCE
	@echo
	@echo ==== Starting to pull components ==========
	@echo
ifneq ($(DONT_REPULL), 1) 
ifeq ($(HAVE_CCONF), 1)
	@if [ -d $(CORECONFDIR)/ ]; then \
		echo "coreconf $(CORECONFDIR) directory found"; \
	else \
		echo "No $(CORECONFDIR) found checking for alternate location"; \
		if [ -d $(ALTCORECONFDIR)/ ]; then \
			echo "found $(ALTCORECONFDIR)....copying to $(CORECONFDIR)/"; \
			cp -r $(ALTCORECONFDIR) $(CORECONFDIR); \
		else \
			echo "No coreconf directory found"; \
			exit 1; \
		fi \
	fi
	@if [ -d $(CORECONFDIR)/ ]; then \
		cd $(CORECONFDIR); \
		gmake; \
	else \
		echo "No $(CORECONFDIR) directory found"; \
	fi
#	cd $(CORECONFDIR)/dbm; \
#	$(MAKE) VERSION=$(DBM_RELEASE_TAG)     $(MFLAGS) $(COMP_PULL_ARG) import
	cd $(CORECONFDIR)/nspr20; \
	$(MAKE) VERSION=$(NSPR_RELEASE_TAG)    $(MFLAGS) $(COMP_PULL_ARG) import
	cd $(CORECONFDIR)/security; \
	$(MAKE) VERSION=$(NSS_RELEASE_TAG)     $(MFLAGS) $(COMP_PULL_ARG) import
	cd $(CORECONFDIR)/svrcore; \
	$(MAKE) VERSION=$(SVRCORE_RELEASE_TAG) $(MFLAGS) $(COMP_PULL_ARG) import
endif
endif

clean clobber:	FORCE
	@if [ -d $(LDAP_DEPTH)/c-sdk/ldap ]; then \
		cd $(LDAP_DEPTH)/c-sdk/ldap; \
		$(MAKE) -f Makefile.client $(MFLAGS) clean; \
	else \
		echo "No LDAP directory -- skipping"; \
		exit 0; \
	fi
	@if [ -d $(CONFIGDIR) ]; then \
		cd $(CONFIGDIR); \
		gmake cleanconfig; \
	fi

FORCE:
