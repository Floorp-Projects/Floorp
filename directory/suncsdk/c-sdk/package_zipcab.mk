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
# This one for external shipments

DEPTH	= ../..
NSPR_TREE = .
MOD_DEPTH = .

include $(NSPR_TREE)/config/rules.mk
include build.mk

ZIPCAB_NAME = ldapcsdk
MMDD	= $(shell date +%m.%d)
BASEINSTDIR = ../../dist/$(MMDD)/$(OBJDIR_NAME)/zipcab
INSTDIR = $(BASEINSTDIR)/$(ZIPCAB_NAME)
ZIPCAB_VERSION = $(shell expr "scale=2; $(VENDOR_VERSION)/100" | bc)
ifndef ZIPCAB_BUILD
ZIPCAB_BUILD = $(shell date +%Y%m%d)-1
endif
LIBDIR  = ../../dist/$(OBJDIR_NAME)/lib
INCDIR  = ../../dist/public/ldap
BINDIR  = ../../dist/$(OBJDIR_NAME)/bin
ETCDIR  = ../../dist/$(OBJDIR_NAME)/etc
EXPDIR  = ldap/examples

all::   FORCE
	$(NSINSTALL) -D $(INSTDIR)
	$(NSINSTALL) -D $(INSTDIR)/lib

# zipcabs are Windows only

	@echo "Installing libraries"
	$(NSINSTALL) $(LIBDIR)/$(LDAP_LIBNAME).$(DLL_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(SSLDAP_LIBNAME).$(DLL_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(PRLDAP_LIBNAME).$(DLL_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(LDAP_LIBNAME).$(LIB_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(SSLDAP_LIBNAME).$(LIB_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(PRLDAP_LIBNAME).$(LIB_SUFFIX) $(INSTDIR)/lib

	@echo "Installing tools"
	$(NSINSTALL) -D $(INSTDIR)/bin
	$(NSINSTALL) $(BINDIR)/ldapsearch$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(BINDIR)/ldapdelete$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(BINDIR)/ldapmodify$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(BINDIR)/ldapcmp$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(BINDIR)/ldapcompare$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(BINDIR)/ldappasswd$(EXE_SUFFIX) $(INSTDIR)/bin

	@echo "Installing includes"
	$(NSINSTALL) -D $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/disptmpl.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/lber.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap-standard.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap-platform.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap-extension.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap-deprecated.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap-to-be-deprecated.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldap_ssl.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/ldappr.h $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/srchpref.h $(INSTDIR)/include

	@echo "Installing etc files"
	$(NSINSTALL) -D $(INSTDIR)/etc
	$(NSINSTALL) $(ETCDIR)/ldapfilter.conf $(INSTDIR)/etc
	$(NSINSTALL) $(ETCDIR)/ldapfriendly $(INSTDIR)/etc
	$(NSINSTALL) $(ETCDIR)/ldapsearchprefs.conf $(INSTDIR)/etc
	$(NSINSTALL) $(ETCDIR)/ldaptemplates.conf $(INSTDIR)/etc

	@echo "Installing example files"
	$(NSINSTALL) -D $(INSTDIR)/examples
	$(NSINSTALL) $(EXPDIR)/*.c $(INSTDIR)/examples
	$(NSINSTALL) $(EXPDIR)/*.h $(INSTDIR)/examples
	$(NSINSTALL) $(EXPDIR)/README $(INSTDIR)/examples
	$(NSINSTALL) $(EXPDIR)/win32.mak $(INSTDIR)/examples
	$(NSINSTALL) $(EXPDIR)/xmplflt.conf $(INSTDIR)/examples

	@echo "Creating preliminary zipcab in $(BASEINSTDIR) directory"
	cd $(BASEINSTDIR) && zip -D -q -r -X $(ZIPCAB_NAME).zip $(ZIPCAB_NAME)
	zipinfo -1 $(BASEINSTDIR)/$(ZIPCAB_NAME).zip > $(BASEINSTDIR)/filelist.txt
	@echo $(ZIPCAB_VERSION)-$(ZIPCAB_BUILD) > $(BASEINSTDIR)/version
	$(RM) $(BASEINSTDIR)/$(ZIPCAB_NAME).zip
	@echo "Creating final zipcab in $(BASEINSTDIR) directory"
	cd $(BASEINSTDIR) && \
	zip -r -X $(ZIPCAB_NAME).zip version filelist.txt $(ZIPCAB_NAME)

ifdef BUILD_SHIP
	@echo "Copying zipcab to $(BUILD_SHIP) directory"
	cp $(BASEINSTDIR)/$(ZIPCAB_NAME).zip $(BUILD_SHIP)/$(OBJDIR_NAME)/
endif

clean:: FORCE
	@echo "Cleaning up old install"
	$(RM) -rf $(BASEINSTDIR)

FORCE::
