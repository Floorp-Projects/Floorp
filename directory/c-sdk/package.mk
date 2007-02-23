# 
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

DEPTH	= ../..
NSPR_TREE = .
MOD_DEPTH = .

include $(NSPR_TREE)/config/rules.mk
include build.mk

NSINSTALL = ./config/nsinstall
ifeq ($(USE_64), 1)
OBJDIR_NAME = $(OS_ARCH)$(OS_RELEASE)_$(CPU_ARCH)_64$(OBJDIR_TAG)
else
OBJDIR_NAME = $(OS_ARCH)$(OS_RELEASE)_$(CPU_ARCH)$(OBJDIR_TAG)
endif
BASEINSTDIR = ../../dist/pkg/$(OBJDIR_NAME)
PKG_NAME_PREFIX = ldapcsdk-
PKG_VERSION = $(shell expr "scale=2; $(VENDOR_VERSION)/100" | bc)
INSTDIR = ../../dist/pkg/$(OBJDIR_NAME)/$(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME)
LDAP_DIST = ../../dist
SASL_DIST = ../../dist
SEC_DIST = ../../dist/$(OBJDIR_NAME)
SVRCORE_DIST = ../../dist
NSPRINCDIR  = $(SEC_DIST)/include
NSSINCDIR  = ../../dist/public/nss
SECLIBDIR  = $(SEC_DIST)/lib
SASLLIBDIR = $(SASL_DIST)/lib
SASLINCDIR = $(SASL_DIST)/include
ifeq ($(OS_ARCH), WINNT)
SASL_BASENAME = libsasl
else
SASL_BASENAME = libsasl2
endif
SVRCORELIBDIR = $(SVRCORE_DIST)/lib
SVRCOREINCDIR = $(SVRCORE_DIST)/include
LDAPLIBDIR  = $(LDAP_DIST)/lib
LDAPINCDIR  = $(LDAP_DIST)/public/ldap
LDAPPRIVATEINCDIR  = $(LDAP_DIST)/public/ldap-private
LDAPBINDIR  = $(LDAP_DIST)/bin
LDAPETCDIR  = $(LDAP_DIST)/etc
LDAPEXPDIR  = ldap/examples
FREEBL_LIBNAME  = freebl*

all::   FORCE
	$(NSINSTALL) -D $(INSTDIR)
	@echo "Installing libraries"
	$(NSINSTALL) -D $(INSTDIR)/lib
# Windows
ifeq ($(OS_ARCH), WINNT)
	$(NSINSTALL) $(LDAPLIBDIR)/$(LDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/$(SSLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/$(PRLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/$(LDIF_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/$(LBER_LIBNAME).lib $(INSTDIR)/lib
	$(NSINSTALL) $(SVRCORELIBDIR)/$(SVRCORE_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/$(NSS_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/$(SSL_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/$(SOFTOKN_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/$(PLC_BASENAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/$(PLDS_BASENAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/$(NSPR_BASENAME).* $(INSTDIR)/lib
ifeq ($(HAVE_SASL), 1)
	$(NSINSTALL) $(SASLLIBDIR)/$(SASL_BASENAME)* $(INSTDIR)/lib
endif
# UNIX
else
	$(NSINSTALL) $(LDAPLIBDIR)/lib$(LDAP_LIBNAME).$(DLL_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/lib$(SSLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/lib$(PRLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/lib$(LDAP_LIBNAME).$(LIB_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/lib$(LBER_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LDAPLIBDIR)/lib$(LDIF_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SVRCORELIBDIR)/lib$(SVRCORE_LIBNAME).$(DLL_SUFFIX)* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/lib$(NSS_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/lib$(SSL_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/lib$(SOFTOKN_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/lib$(PLC_BASENAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/lib$(PLDS_BASENAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(SECLIBDIR)/lib$(NSPR_BASENAME).* $(INSTDIR)/lib
ifeq ($(HAVE_SASL), 1)
        ifeq ($(OS_ARCH), HP-UX)
		$(NSINSTALL) $(SASLLIBDIR)/$(SASL_BASENAME).sl* $(INSTDIR)/lib
		$(NSINSTALL) $(SASLLIBDIR)/sasl2/*.sl $(INSTDIR)/lib
        else
		$(NSINSTALL) $(SASLLIBDIR)/$(SASL_BASENAME).so* $(INSTDIR)/lib
		$(NSINSTALL) $(SASLLIBDIR)/sasl2/*.so* $(INSTDIR)/lib
        endif
endif
endif

ifeq ($(COPYFREEBL), 1)
ifeq ($(OS_ARCH), WINNT)
	$(NSINSTALL) $(SECLIBDIR)/$(FREEBL_LIBNAME).* $(INSTDIR)/lib
else
	$(NSINSTALL) $(SECLIBDIR)/lib$(FREEBL_LIBNAME).* $(INSTDIR)/lib
endif
endif

ifneq ($(OS_ARCH), WINNT)
	rm -f $(INSTDIR)/lib/*.a
endif

	@echo "Installing tools"
	$(NSINSTALL) -D $(INSTDIR)/bin
	$(NSINSTALL) $(LDAPBINDIR)/ldapsearch$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(LDAPBINDIR)/ldapdelete$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(LDAPBINDIR)/ldapmodify$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(LDAPBINDIR)/ldapcmp$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(LDAPBINDIR)/ldapcompare$(EXE_SUFFIX) $(INSTDIR)/bin
	$(NSINSTALL) $(LDAPBINDIR)/ldappasswd$(EXE_SUFFIX) $(INSTDIR)/bin

	@echo "Installing header files"
	$(NSINSTALL) -D $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/disptmpl.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/lber.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldif.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap-standard.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap-platform.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap-extension.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap-deprecated.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap-to-be-deprecated.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldap_ssl.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/ldappr.h $(INSTDIR)/include
	$(NSINSTALL) $(LDAPINCDIR)/srchpref.h $(INSTDIR)/include

	$(NSINSTALL) -D $(INSTDIR)/include
	cp -r $(SVRCOREINCDIR)/svrcore.h $(INSTDIR)/include

	$(NSINSTALL) -D $(INSTDIR)/include
	cp -r $(NSPRINCDIR)/* $(INSTDIR)/include
	rm -rf $(INSTDIR)/include/md $(INSTDIR)/include/obsolete $(INSTDIR)/include/private

	$(NSINSTALL) -D $(INSTDIR)/include
	cp -r $(NSSINCDIR)/* $(INSTDIR)/include

ifeq ($(HAVE_SASL), 1)
	$(NSINSTALL) $(SASLINCDIR)/sasl/*.h $(INSTDIR)/include
endif

	@echo "Installing etc files"
	$(NSINSTALL) -D $(INSTDIR)/etc
	$(NSINSTALL) $(LDAPETCDIR)/ldapfilter.conf $(INSTDIR)/etc
	$(NSINSTALL) $(LDAPETCDIR)/ldapfriendly $(INSTDIR)/etc
	$(NSINSTALL) $(LDAPETCDIR)/ldapsearchprefs.conf $(INSTDIR)/etc
	$(NSINSTALL) $(LDAPETCDIR)/ldaptemplates.conf $(INSTDIR)/etc

	@echo "Installing example files"
	$(NSINSTALL) -D $(INSTDIR)/examples
	$(NSINSTALL) $(LDAPEXPDIR)/*.c $(INSTDIR)/examples
	$(NSINSTALL) $(LDAPEXPDIR)/*.h $(INSTDIR)/examples
	$(NSINSTALL) $(LDAPEXPDIR)/README $(INSTDIR)/examples
ifneq ($(OS_ARCH), WINNT)
	$(NSINSTALL) $(LDAPEXPDIR)/Makefile $(INSTDIR)/examples
endif
ifeq ($(OS_ARCH), WINNT)
	$(NSINSTALL) $(LDAPEXPDIR)/win32.mak $(INSTDIR)/examples
endif
	$(NSINSTALL) $(LDAPEXPDIR)/xmplflt.conf $(INSTDIR)/examples

	@echo "Creating tarball in $(BASEINSTDIR) directory"
ifeq ($(OS_ARCH), WINNT)
	cd $(BASEINSTDIR) && zip -r -X $(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).zip \
		$(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME)
else
	cd $(BASEINSTDIR) && tar -cvf $(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).tar \
		$(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME) \
		&& gzip -c $(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).tar > \
		    $(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).tgz && rm -f \
			  $(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).tar
endif

ifdef BUILD_SHIP
	@echo "Copying tarball to $(BUILD_SHIP) directory"
ifeq ($(OS_ARCH), WINNT)
	cp $(BASEINSTDIR)/$(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).zip $(BUILD_SHIP)
else
	cp $(BASEINSTDIR)/$(PKG_NAME_PREFIX)$(PKG_VERSION)-$(OBJDIR_NAME).tgz $(BUILD_SHIP)
endif
endif

clean:: FORCE
	@echo "Cleaning up old install"
	$(RM) -rf $(BASEINSTDIR)

FORCE::
