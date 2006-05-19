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

DEPTH	= ../..
NSPR_TREE = .
MOD_DEPTH = .

include $(NSPR_TREE)/config/rules.mk
include build.mk

MMDD	= $(shell date +%m.%d)
INSTDIR = ../../dist/$(MMDD)/$(OBJDIR_NAME)
LIBDIR  = ../../dist/$(OBJDIR_NAME)/lib
INCDIR  = ../../dist/public/ldap
PRIVATEINCDIR  = ../../dist/public/ldap-private
NSPRINCDIR  = ../../dist/$(OBJDIR_NAME)/include
BINDIR  = ../../dist/$(OBJDIR_NAME)/bin
ETCDIR  = ../../dist/$(OBJDIR_NAME)/etc
EXPDIR  = ldap/examples
DOCDIR  = ldap/docs

# defaults
PKG_PRIVATE_HDRS=1
PKG_PRIVATE_LIBS=1
PKG_DEP_LIBS=1

all::   FORCE
	$(NSINSTALL) -D $(INSTDIR)
	@echo "Installing libraries"
	$(NSINSTALL) -D $(INSTDIR)/lib
# only the libraries we produce go in lib - everyone else in lib-private
	$(NSINSTALL) -D $(INSTDIR)/lib-private
# Windows
ifeq ($(OS_ARCH), WINNT)
  ifeq ($(HAVE_LIBICU),1)
    ifneq ($(HAVE_LIBICU_LOCAL),1)
			$(NSINSTALL) $(LIBDIR)/../libicu/icudt32.$(DLL_SUFFIX) $(INSTDIR)/lib-private
			$(NSINSTALL) $(LIBDIR)/../libicu/icuuc32.$(DLL_SUFFIX) $(INSTDIR)/lib-private
			$(NSINSTALL) $(LIBDIR)/../libicu/icuin32.$(DLL_SUFFIX) $(INSTDIR)/lib-private
    else
		@echo ""
		@echo "WARNING: HAVE_LIBICU_LOCAL is set for libicu; excluding from the package"
		@echo ""
    endif
  endif
	$(NSINSTALL) $(LIBDIR)/$(LDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(SSLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(PRLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(LBER_LIBNAME).lib $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/$(LDIF_LIBNAME).lib $(INSTDIR)/lib
  ifeq ($(PKG_PRIVATE_LIBS),1)
		$(NSINSTALL) $(LIBDIR)/$(IUTIL_LIBNAME).lib $(INSTDIR)/lib
  endif
	$(NSINSTALL) $(LIBDIR)/$(UTIL_LIBNAME).lib $(INSTDIR)/lib
  ifeq ($(PKG_DEP_LIBS),1)
		$(NSINSTALL) $(LIBDIR)/$(NSS_LIBNAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(SSL_LIBNAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(STKN_LIBNAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(PLC_BASENAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(PLDS_BASENAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(NSPR_BASENAME).* $(INSTDIR)/lib-private
    ifeq ($(HAVE_SASL), 1)
      ifneq ($(HAVE_SASL_LOCAL), 1)
#				$(NSINSTALL) $(LIBDIR)/$(SASL_BASENAME).dll $(INSTDIR)/lib-private
#				$(NSINSTALL) $(LIBDIR)/$(SASL_BASENAME).lib $(INSTDIR)/lib-private
				$(NSINSTALL) ../../dist/$(OBJDIR_NAME)/libsasl/*.dll $(INSTDIR)/lib-private
				$(NSINSTALL) ../../dist/$(OBJDIR_NAME)/libsasl/*.lib $(INSTDIR)/lib-private
      else
      	@echo ""
		@echo "WARNING: HAVE_SASL_LOCAL is set for libsasl; excluding from the package"
		@echo ""
      endif
    endif
  endif
# UNIX
else
ifeq ($(HAVE_LIBICU),1)
  ifneq ($(HAVE_LIBICU_LOCAL),1)
    ifneq ($(OS_ARCH),AIX)
		$(NSINSTALL) $(LIBDIR)/../libicu/libicudata.$(DLL_SUFFIX).3 $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/../libicu/libicuuc.$(DLL_SUFFIX).3 $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/../libicu/libicui18n.$(DLL_SUFFIX).3 $(INSTDIR)/lib-private
    else
		$(NSINSTALL) $(LIBDIR)/../libicu/libicudata.$(DLL_SUFFIX).2 $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/../libicu/libicuuc.$(DLL_SUFFIX).2 $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/../libicu/libicui18n.$(DLL_SUFFIX).2 $(INSTDIR)/lib-private
    endif
  else
	@echo ""
	@echo "WARNING: HAVE_LIBICU_LOCAL is set for libicu; excluding from the package"
	@echo ""
    endif
  endif
	$(NSINSTALL) $(LIBDIR)/lib$(LDAP_LIBNAME).$(DLL_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/lib$(SSLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/lib$(PRLDAP_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/lib$(LDAP_LIBNAME).$(LIB_SUFFIX) $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/lib$(LBER_LIBNAME).* $(INSTDIR)/lib
	$(NSINSTALL) $(LIBDIR)/lib$(LDIF_LIBNAME).* $(INSTDIR)/lib
  ifeq ($(PKG_DEP_LIBS),1)
		$(NSINSTALL) $(LIBDIR)/lib$(NSS_LIBNAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/lib$(SSL_LIBNAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/lib$(STKN_LIBNAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(PLC_BASENAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(PLDS_BASENAME).* $(INSTDIR)/lib-private
		$(NSINSTALL) $(LIBDIR)/$(NSPR_BASENAME).* $(INSTDIR)/lib-private
    ifeq ($(HAVE_SASL), 1)
      ifneq ($(HAVE_SASL_LOCAL),1)
#	  		$(NSINSTALL) $(LIBDIR)/lib$(SASL_LIBNAME).* $(INSTDIR)/lib-private
        ifeq ($(OS_ARCH), HP-UX)
				$(NSINSTALL) $(LIBSASL_LIB_LOC)/*.sl $(INSTDIR)/lib-private
        else
				$(NSINSTALL) $(LIBSASL_LIB_LOC)/*.so $(INSTDIR)/lib-private
        endif
      else
		@echo ""
		@echo "WARNING: HAVE_SASL_LOCAL is set for libsasl; excluding from the package"
		@echo ""
      endif
    endif
  endif
endif

ifeq ($(COPYFREEBL), 1)
ifeq ($(OS_ARCH), WINNT)
	$(NSINSTALL) $(LIBDIR)/$(FREEBL_LIBNAME) $(INSTDIR)/lib-private
else
	$(NSINSTALL) $(LIBDIR)/lib$(FREEBL_LIBNAME) $(INSTDIR)/lib-private
endif
endif

	@echo "Installing tools"
	$(NSINSTALL) -D $(INSTDIR)/tools
	$(NSINSTALL) $(BINDIR)/ldapsearch$(EXE_SUFFIX) $(INSTDIR)/tools
	$(NSINSTALL) $(BINDIR)/ldapdelete$(EXE_SUFFIX) $(INSTDIR)/tools
	$(NSINSTALL) $(BINDIR)/ldapmodify$(EXE_SUFFIX) $(INSTDIR)/tools
	$(NSINSTALL) $(BINDIR)/ldapcmp$(EXE_SUFFIX) $(INSTDIR)/tools
	$(NSINSTALL) $(BINDIR)/ldapcompare$(EXE_SUFFIX) $(INSTDIR)/tools
	$(NSINSTALL) $(BINDIR)/ldappasswd$(EXE_SUFFIX) $(INSTDIR)/tools

	@echo "Installing includes"
	$(NSINSTALL) -D $(INSTDIR)/include
	$(NSINSTALL) $(INCDIR)/disptmpl.h $(INSTDIR)/include
ifeq ($(PKG_PRIVATE_LIBS),1)
	$(NSINSTALL) $(INCDIR)/iutil.h $(INSTDIR)/include
endif
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

ifeq ($(PKG_PRIVATE_HDRS),1)
	@echo "Installing private include files"
	$(NSINSTALL) -D $(INSTDIR)/include-private
	$(NSINSTALL) -D $(INSTDIR)/include-private/liblber
	$(NSINSTALL) $(PRIVATEINCDIR)/lber-int.h $(INSTDIR)/include-private/liblber
	$(NSINSTALL) $(PRIVATEINCDIR)/ldap-int.h $(INSTDIR)/include-private
	$(NSINSTALL) $(PRIVATEINCDIR)/portable.h $(INSTDIR)/include-private
	$(NSINSTALL) $(PRIVATEINCDIR)/ldaprot.h $(INSTDIR)/include-private
	$(NSINSTALL) $(PRIVATEINCDIR)/ldaplog.h $(INSTDIR)/include-private

	@echo "Installing nspr header files"
	$(NSINSTALL) -D $(INSTDIR)/include-nspr
	cp -r $(NSPRINCDIR)/* $(INSTDIR)/include-nspr
endif

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
	$(NSINSTALL) $(EXPDIR)/Makefile $(INSTDIR)/examples
	$(NSINSTALL) $(EXPDIR)/xmplflt.conf $(INSTDIR)/examples

ifdef BUILD_SHIP
	@echo "Copying files to $(BUILD_SHIP) directory"
	cp -r $(INSTDIR) $(BUILD_SHIP)
endif

clean:: FORCE
	@echo "Cleaning up old install"
	$(RM) -rf $(INSTDIR)

FORCE::
