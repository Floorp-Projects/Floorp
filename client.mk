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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Stephen Lamm
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

# Build the Mozilla client.
#
# This needs CVSROOT set to work, e.g.,
#   setenv CVSROOT :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
# or
#   setenv CVSROOT :pserver:username%somedomain.org@cvs.mozilla.org:/cvsroot
# 
# To checkout and build a tree,
#    1. cvs co mozilla/client.mk
#    2. cd mozilla
#    3. gmake -f client.mk
#
# Other targets (gmake -f client.mk [targets...]),
#    checkout
#    build
#    clean (realclean is now the same as clean)
#    distclean
#
# See http://www.mozilla.org/build/unix.html for more information.
#
# Options:
#   MOZ_OBJDIR           - Destination object directory
#   MOZ_CO_DATE          - Date tag to use for checkout (default: none)
#   MOZ_CO_MODULE        - Module to checkout (default: SeaMonkeyAll)
#   MOZ_CVS_FLAGS        - Flags to pass cvs (default: -q -z3)
#   MOZ_CO_FLAGS         - Flags to pass after 'cvs co' (default: -P)
#   MOZ_MAKE_FLAGS       - Flags to pass to $(MAKE)
#   MOZ_CO_BRANCH        - Branch tag (Deprecated. Use MOZ_CO_TAG below.)
#   MOZ_CO_LOCALES       - localizations to pull (MOZ_CO_LOCALES="de-DE pt-BR")
#   LOCALES_CVSROOT      - CVSROOT to use to pull localizations
#

#######################################################################
# Checkout Tags
#
# For branches, uncomment the MOZ_CO_TAG line with the proper tag,
# and commit this file on that tag.
#MOZ_CO_TAG = <tag>
NSPR_CO_TAG = NSPRPUB_PRE_4_2_CLIENT_BRANCH
PSM_CO_TAG = #We will now build PSM from the tip instead of a branch.
NSS_CO_TAG = NSS_CLIENT_TAG
LDAPCSDK_CO_TAG = ldapcsdk_50_client_branch
ACCESSIBLE_CO_TAG = 
IMGLIB2_CO_TAG = 
IPC_CO_TAG = 
TOOLKIT_CO_TAG =
BROWSER_CO_TAG =
MAIL_CO_TAG =
STANDALONE_COMPOSER_CO_TAG =
LOCALES_CO_TAG =
BUILD_MODULES = all

#######################################################################
# Defines
#
CVS = cvs

CWD := $(shell pwd)

ifeq "$(CWD)" "/"
CWD   := /.
endif

ifneq (, $(wildcard client.mk))
# Ran from mozilla directory
ROOTDIR   := $(shell dirname $(CWD))
TOPSRCDIR := $(CWD)
else
# Ran from mozilla/.. directory (?)
ROOTDIR   := $(CWD)
TOPSRCDIR := $(CWD)/mozilla
endif

# on os2, TOPSRCDIR may have two forward slashes in a row, which doesn't
#  work;  replace first instance with one forward slash
TOPSRCDIR := $(shell echo "$(TOPSRCDIR)" | sed -e 's%//%/%')

ifndef TOPSRCDIR_MOZ
TOPSRCDIR_MOZ=$(TOPSRCDIR)
endif

# if ROOTDIR equals only drive letter (i.e. "C:"), set to "/"
DIRNAME := $(shell echo "$(ROOTDIR)" | sed -e 's/^.://')
ifeq ($(DIRNAME),)
ROOTDIR := /.
endif

AUTOCONF := autoconf
MKDIR := mkdir
SH := /bin/sh
ifndef MAKE
MAKE := gmake
endif

CONFIG_GUESS_SCRIPT := $(wildcard $(TOPSRCDIR)/build/autoconf/config.guess)
ifdef CONFIG_GUESS_SCRIPT
  CONFIG_GUESS = $(shell $(CONFIG_GUESS_SCRIPT))
else
  _IS_FIRST_CHECKOUT := 1
endif

####################################
# CVS

# Add the CVS root to CVS_FLAGS if needed
CVS_ROOT_IN_TREE := $(shell cat $(TOPSRCDIR)/CVS/Root 2>/dev/null)
ifneq ($(CVS_ROOT_IN_TREE),)
ifneq ($(CVS_ROOT_IN_TREE),$(CVSROOT))
  CVS_FLAGS := -d $(CVS_ROOT_IN_TREE)
endif
endif

CVSCO = $(strip $(CVS) $(CVS_FLAGS) co $(CVS_CO_FLAGS))
CVSCO_LOGFILE := $(ROOTDIR)/cvsco.log
CVSCO_LOGFILE := $(shell echo $(CVSCO_LOGFILE) | sed s%//%/%)

ifdef MOZ_CO_TAG
  CVS_CO_FLAGS := -r $(MOZ_CO_TAG)
endif

# if LOCALES_CVSROOT is not specified, set it here
# (and let mozconfig override it)
LOCALES_CVSROOT ?= :pserver:anonymous@cvs-mirror.mozilla.org:/l10n

####################################
# Load mozconfig Options

# See build pages, http://www.mozilla.org/build/unix.html, 
# for how to set up mozconfig.
MOZCONFIG_LOADER := mozilla/build/autoconf/mozconfig2client-mk
MOZCONFIG_FINDER := mozilla/build/autoconf/mozconfig-find 
MOZCONFIG_MODULES := mozilla/build/unix/modules.mk mozilla/build/unix/uniq.pl
run_for_side_effects := \
  $(shell cd $(ROOTDIR); \
     if test "$(_IS_FIRST_CHECKOUT)"; then \
        $(CVSCO) $(MOZCONFIG_FINDER) $(MOZCONFIG_LOADER) $(MOZCONFIG_MODULES); \
     else true; \
     fi; \
     $(MOZCONFIG_LOADER) $(TOPSRCDIR) mozilla/.mozconfig.mk > mozilla/.mozconfig.out)
include $(TOPSRCDIR)/.mozconfig.mk
include $(TOPSRCDIR)/build/unix/modules.mk

####################################
# Options that may come from mozconfig

# Change CVS flags if anonymous root is requested
ifdef MOZ_CO_USE_MIRROR
  CVS_FLAGS := -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
endif

# MOZ_CVS_FLAGS - Basic CVS flags
ifeq "$(origin MOZ_CVS_FLAGS)" "undefined"
  CVS_FLAGS := $(CVS_FLAGS) -q -z 3 
else
  CVS_FLAGS := $(MOZ_CVS_FLAGS)
endif

# This option is deprecated. The best way to have client.mk pull a tag
# is to set MOZ_CO_TAG (see above) and commit that change on the tag.
ifdef MOZ_CO_BRANCH
  $(warning Use MOZ_CO_TAG instead of MOZ_CO_BRANCH)
  CVS_CO_FLAGS := -r $(MOZ_CO_BRANCH)
endif

# MOZ_CO_FLAGS - Anything that we should use on all checkouts
ifeq "$(origin MOZ_CO_FLAGS)" "undefined"
  CVS_CO_FLAGS := $(CVS_CO_FLAGS) -P
else
  CVS_CO_FLAGS := $(CVS_CO_FLAGS) $(MOZ_CO_FLAGS)
endif

ifdef MOZ_CO_DATE
  CVS_CO_DATE_FLAGS := -D "$(MOZ_CO_DATE)"
endif

ifdef MOZ_OBJDIR
  OBJDIR := $(MOZ_OBJDIR)
  MOZ_MAKE := $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)
else
  OBJDIR := $(TOPSRCDIR)
  MOZ_MAKE := $(MAKE) $(MOZ_MAKE_FLAGS)
endif

####################################
# CVS defines for PSM
#
PSM_CO_MODULE= mozilla/security/manager
PSM_CO_FLAGS := -P -A
ifdef MOZ_CO_FLAGS
  PSM_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef PSM_CO_TAG
  PSM_CO_FLAGS := $(PSM_CO_FLAGS) -r $(PSM_CO_TAG)
endif
CVSCO_PSM = $(CVS) $(CVS_FLAGS) co $(PSM_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(PSM_CO_MODULE)

####################################
# CVS defines for NSS
#
NSS_CO_MODULE = mozilla/security/nss \
		mozilla/security/coreconf \
		$(NULL)

NSS_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  NSS_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef NSS_CO_TAG
   NSS_CO_FLAGS := $(NSS_CO_FLAGS) -r $(NSS_CO_TAG)
endif
# Cannot pull static tags by date
ifeq ($(NSS_CO_TAG),NSS_CLIENT_TAG)
CVSCO_NSS = $(CVS) $(CVS_FLAGS) co $(NSS_CO_FLAGS) $(NSS_CO_MODULE)
else
CVSCO_NSS = $(CVS) $(CVS_FLAGS) co $(NSS_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(NSS_CO_MODULE)
endif

####################################
# CVS defines for NSPR
#
NSPR_CO_MODULE = mozilla/nsprpub
NSPR_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  NSPR_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef NSPR_CO_TAG
  NSPR_CO_FLAGS := $(NSPR_CO_FLAGS) -r $(NSPR_CO_TAG)
endif
# Cannot pull static tags by date
ifeq ($(NSPR_CO_TAG),NSPRPUB_CLIENT_TAG)
CVSCO_NSPR = $(CVS) $(CVS_FLAGS) co $(NSPR_CO_FLAGS) $(NSPR_CO_MODULE)
else
CVSCO_NSPR = $(CVS) $(CVS_FLAGS) co $(NSPR_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(NSPR_CO_MODULE)
endif

####################################
# CVS defines for the C LDAP SDK
#
LDAPCSDK_CO_MODULE = mozilla/directory/c-sdk
LDAPCSDK_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  LDAPCSDK_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef LDAPCSDK_CO_TAG
  LDAPCSDK_CO_FLAGS := $(LDAPCSDK_CO_FLAGS) -r $(LDAPCSDK_CO_TAG)
endif
CVSCO_LDAPCSDK = $(CVS) $(CVS_FLAGS) co $(LDAPCSDK_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(LDAPCSDK_CO_MODULE)

####################################
# CVS defines for the C LDAP SDK
#
ACCESSIBLE_CO_MODULE = mozilla/accessible
ACCESSIBLE_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  ACCESSIBLE_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef ACCESSIBLE_CO_TAG
  ACCESSIBLE_CO_FLAGS := $(ACCESSIBLE_CO_FLAGS) -r $(ACCESSIBLE_CO_TAG)
endif
CVSCO_ACCESSIBLE = $(CVS) $(CVS_FLAGS) co $(ACCESSIBLE_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(ACCESSIBLE_CO_MODULE)


####################################
# CVS defines for new image library
#
IMGLIB2_CO_MODULE = mozilla/modules/libpr0n
IMGLIB2_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  IMGLIB2_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef IMGLIB2_CO_TAG
  IMGLIB2_CO_FLAGS := $(IMGLIB2_CO_FLAGS) -r $(IMGLIB2_CO_TAG)
endif
CVSCO_IMGLIB2 = $(CVS) $(CVS_FLAGS) co $(IMGLIB2_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(IMGLIB2_CO_MODULE)

####################################
# CVS defines for ipc module
#
IPC_CO_MODULE = mozilla/ipc/ipcd
IPC_CO_FLAGS := -P -A
ifdef MOZ_CO_FLAGS
  IPC_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef IPC_CO_TAG
  IPC_CO_FLAGS := $(IPC_CO_FLAGS) -r $(IPC_CO_TAG)
endif
CVSCO_IPC = $(CVS) $(CVS_FLAGS) co $(IPC_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(IPC_CO_MODULE)

####################################
# CVS defines for Calendar 
#
CVSCO_CALENDAR := $(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/calendar mozilla/other-licenses/libical

####################################
# CVS defines for SeaMonkey
#
ifeq ($(MOZ_CO_MODULE),)
  MOZ_CO_MODULE := SeaMonkeyAll
endif
CVSCO_SEAMONKEY := $(CVSCO) $(CVS_CO_DATE_FLAGS) $(MOZ_CO_MODULE)

####################################
# CVS defines for standalone modules
#
ifeq ($(BUILD_MODULES),all)
  CHECKOUT_STANDALONE := true
  CHECKOUT_STANDALONE_NOSUBDIRS := true
else
  STANDALONE_CO_MODULE := $(filter-out $(NSPRPUB_DIR) security directory/c-sdk, $(BUILD_MODULE_CVS))
  STANDALONE_CO_MODULE += allmakefiles.sh client.mk aclocal.m4 configure configure.in
  STANDALONE_CO_MODULE += Makefile.in
  STANDALONE_CO_MODULE := $(addprefix mozilla/, $(STANDALONE_CO_MODULE))
  CHECKOUT_STANDALONE := cvs_co $(CVSCO) $(CVS_CO_DATE_FLAGS) $(STANDALONE_CO_MODULE)

  NOSUBDIRS_MODULE := $(addprefix mozilla/, $(BUILD_MODULE_CVS_NS))
ifneq ($(NOSUBDIRS_MODULE),)
  CHECKOUT_STANDALONE_NOSUBDIRS := cvs_co $(CVSCO) -l $(CVS_CO_DATE_FLAGS) $(NOSUBDIRS_MODULE)
else
  CHECKOUT_STANDALONE_NOSUBDIRS := true
endif

CVSCO_SEAMONKEY :=
ifeq (,$(filter $(NSPRPUB_DIR), $(BUILD_MODULE_CVS)))
  CVSCO_NSPR :=
endif
ifeq (,$(filter security security/manager, $(BUILD_MODULE_CVS)))
  CVSCO_PSM :=
  CVSCO_NSS :=
endif
ifeq (,$(filter directory/c-sdk, $(BUILD_MODULE_CVS)))
  CVSCO_LDAPCSDK :=
endif
ifeq (,$(filter accessible, $(BUILD_MODULE_CVS)))
  CVSCO_ACCESSIBLE :=
endif
ifeq (,$(filter modules/libpr0n, $(BUILD_MODULE_CVS)))
  CVSCO_IMGLIB2 :=
endif
ifeq (,$(filter ipc, $(BUILD_MODULE_CVS)))
  CVSCO_IPC :=
endif
ifeq (,$(filter calendar other-licenses/libical, $(BUILD_MODULE_CVS)))
  CVSCO_CALENDAR :=
endif
endif

####################################
# CVS defined for libart (pulled and built if MOZ_INTERNAL_LIBART_LGPL is set)
#
CVSCO_LIBART := $(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/other-licenses/libart_lgpl

ifdef MOZ_INTERNAL_LIBART_LGPL
FASTUPDATE_LIBART := fast_update $(CVSCO_LIBART)
CHECKOUT_LIBART := cvs_co $(CVSCO_LIBART)
else
CHECKOUT_LIBART := true
FASTUPDATE_LIBART := true
endif

####################################
# CVS defines for Phoenix (pulled and built if MOZ_PHOENIX is set)
#
BROWSER_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  BROWSER_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef BROWSER_CO_TAG
  BROWSER_CO_FLAGS := $(BROWSER_CO_FLAGS) -r $(BROWSER_CO_TAG)
endif

BROWSER_CO_DIRS := mozilla/browser mozilla/other-licenses/branding/firefox mozilla/other-licenses/7zstub/firefox

CVSCO_PHOENIX := $(CVS) $(CVS_FLAGS) co $(BROWSER_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(BROWSER_CO_DIRS)

ifdef MOZ_PHOENIX
FASTUPDATE_PHOENIX := fast_update $(CVSCO_PHOENIX)
CHECKOUT_PHOENIX := cvs_co $(CVSCO_PHOENIX)
MOZ_XUL_APP = 1
LOCALE_DIRS += mozilla/browser/locales
else
CHECKOUT_PHOENIX := true
FASTUPDATE_PHOENIX := true
endif

####################################
# CVS defines for Thunderbird (pulled and built if MOZ_THUNDERBIRD is set)
#

MAIL_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  MAIL_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef MAIL_CO_TAG
  MAIL_CO_FLAGS := $(MAIL_CO_FLAGS) -r $(MAIL_CO_TAG)
endif

MAIL_CO_DIRS := mozilla/mail mozilla/other-licenses/branding/thunderbird mozilla/other-licenses/7zstub/thunderbird

CVSCO_THUNDERBIRD := $(CVS) $(CVS_FLAGS) co $(MAIL_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(MAIL_CO_DIRS)
ifdef MOZ_THUNDERBIRD
FASTUPDATE_THUNDERBIRD := fast_update $(CVSCO_THUNDERBIRD)
CHECKOUT_THUNDERBIRD := cvs_co $(CVSCO_THUNDERBIRD)
MOZ_XUL_APP = 1
LOCALE_DIRS += mozilla/mail/locales
else
FASTUPDATE_THUNDERBIRD := true
CHECKOUT_THUNDERBIRD := true
endif

####################################
# CVS defines for Standalone Composer (pulled and built if MOZ_STANDALONE_COMPOSER is set)
#

STANDALONE_COMPOSER_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  STANDALONE_COMPOSER_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef STANDALONE_COMPOSER_CO_TAG
  STANDALONE_COMPOSER_CO_FLAGS := $(STANDALONE_COMPOSER_CO_FLAGS) -r $(STANDALONE_COMPOSER_CO_TAG)
endif

CVSCO_STANDALONE_COMPOSER := $(CVS) $(CVS_FLAGS) co $(STANDALONE_COMPOSER_CO_FLAGS) $(CVS_CO_DATE_FLAGS) mozilla/composer
ifdef MOZ_STANDALONE_COMPOSER
FASTUPDATE_STANDALONE_COMPOSER:= fast_update $(CVSCO_STANDALONE_COMPOSER)
CHECKOUT_STANDALONE_COMPOSER:= cvs_co $(CVSCO_STANDALONE_COMPOSER)
MOZ_XUL_APP = 1
LOCALE_DIRS += mozilla/composer/locales
else
FASTUPDATE_STANDALONE_COMPOSER:= true
CHECKOUT_STANDALONE_COMPOSER:= true
endif

####################################
# CVS defines for Sunbird (pulled and built if MOZ_SUNBIRD is set)
#

ifdef MOZ_SUNBIRD
MOZ_XUL_APP = 1
endif

####################################
# CVS defines for mozilla/toolkit
#

TOOLKIT_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  TOOLKIT_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
ifdef TOOLKIT_CO_TAG
  TOOLKIT_CO_FLAGS := $(TOOLKIT_CO_FLAGS) -r $(TOOLKIT_CO_TAG)
endif

CVSCO_MOZTOOLKIT := $(CVS) $(CVS_FLAGS) co $(TOOLKIT_CO_FLAGS) $(CVS_CO_DATE_FLAGS)  mozilla/toolkit mozilla/chrome
FASTUPDATE_MOZTOOLKIT := fast_update $(CVSCO_MOZTOOLKIT)
CHECKOUT_MOZTOOLKIT := cvs_co $(CVSCO_MOZTOOLKIT)
LOCALE_DIRS += mozilla/toolkit/locales

####################################
# CVS defines for codesighs (pulled and built if MOZ_MAPINFO is set)
#
CVSCO_CODESIGHS := $(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/tools/codesighs

ifdef MOZ_MAPINFO
FASTUPDATE_CODESIGHS := fast_update $(CVSCO_CODESIGHS)
CHECKOUT_CODESIGHS := cvs_co $(CVSCO_CODESIGHS)
else
CHECKOUT_CODESIGHS := true
FASTUPDATE_CODESIGHS := true
endif

###################################
# CVS defines for locales
#
LOCALES_CO_FLAGS := -P

ifdef LOCALES_CO_TAG
  LOCALES_CO_FLAGS := $(LOCALES_CO_FLAGS) -r $(LOCALES_CO_TAG)
endif

ifndef MOZ_CO_LOCALES
FASTUPDATE_LOCALES := true
CHECKOUT_LOCALES := true
else
ifeq (all,$(MOZ_CO_LOCALES))
MOZCONFIG_MODULES += $(addsuffix /all-locales,$(LOCALE_DIRS))

FASTUPDATE_LOCALES := \
  for dir in $(LOCALE_DIRS); do \
    for locale in `cat $$dir/all-locales`; do \
      fast_update $(CVS) $(CVS_FLAGS) -d $(LOCALES_CVSROOT) co $$dir/$$locale; \
    done; \
  done 

CHECKOUT_LOCALES := \
  for dir in $(LOCALE_DIRS); do \
    for locale in `cat $$dir/all-locales`; do \
      cvs_co $(CVS) $(CVS_FLAGS) -d $(LOCALES_CVSROOT) co $(LOCALES_CO_FLAGS) $$dir/$$locale; \
    done; \
  done

else
LOCALE_CO_DIRS = $(foreach locale,$(MOZ_CO_LOCALES),$(addsuffix /$(locale),$(LOCALE_DIRS)))

CVSCO_LOCALES := $(CVS) $(CVS_FLAGS) -d $(LOCALES_CVSROOT) co $(LOCALES_CO_FLAGS) $(LOCALE_CO_DIRS)

FASTUPDATE_LOCALES := fast_update $(CVSCO_LOCALES)
CHECKOUT_LOCALES := cvs_co $(CVSCO_LOCALES)
endif
endif #MOZ_CO_LOCALES

#######################################################################
# Rules
# 

# Print out any options loaded from mozconfig.
all build checkout clean depend distclean export libs install realclean::
	@if test -f .mozconfig.out; then \
	  cat .mozconfig.out; \
	  rm -f .mozconfig.out; \
	else true; \
	fi

ifdef _IS_FIRST_CHECKOUT
all:: checkout build
else
all:: checkout alldep
endif

# Windows equivalents
pull_all: checkout
build_all: build
build_all_dep: alldep
build_all_depend: alldep
clobber clobber_all: clean
pull_and_build_all: checkout alldep

# Do everything from scratch
everything: checkout clean build

####################################
# CVS checkout
#
checkout::
#	@: Backup the last checkout log.
	@if test -f $(CVSCO_LOGFILE) ; then \
	  mv $(CVSCO_LOGFILE) $(CVSCO_LOGFILE).old; \
	else true; \
	fi
ifdef RUN_AUTOCONF_LOCALLY
	@echo "Removing local configures" ; \
	cd $(ROOTDIR) && \
	$(RM) -f mozilla/configure mozilla/nsprpub/configure \
		mozilla/directory/c-sdk/configure
endif
	@echo "checkout start: "`date` | tee $(CVSCO_LOGFILE)
	@echo '$(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/client.mk $(MOZCONFIG_MODULES)'; \
        cd $(ROOTDIR) && \
	$(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/client.mk $(MOZCONFIG_MODULES)
	@cd $(ROOTDIR) && $(MAKE) -f mozilla/client.mk real_checkout

#	Start the checkout. Split the output to the tty and a log file.

real_checkout:
	@set -e; \
	cvs_co() { set -e; echo "$$@" ; \
	  "$$@" 2>&1 | tee -a $(CVSCO_LOGFILE); }; \
	$(CHECKOUT_STANDALONE); \
	$(CHECKOUT_STANDALONE_NOSUBDIRS); \
	cvs_co $(CVSCO_NSPR); \
	cvs_co $(CVSCO_NSS); \
	cvs_co $(CVSCO_PSM); \
	cvs_co $(CVSCO_LDAPCSDK); \
	cvs_co $(CVSCO_ACCESSIBLE); \
	cvs_co $(CVSCO_IMGLIB2); \
	cvs_co $(CVSCO_IPC); \
	cvs_co $(CVSCO_CALENDAR); \
	$(CHECKOUT_LIBART); \
	$(CHECKOUT_MOZTOOLKIT); \
	$(CHECKOUT_PHOENIX); \
	$(CHECKOUT_THUNDERBIRD); \
	$(CHECKOUT_STANDALONE_COMPOSER); \
	$(CHECKOUT_CODESIGHS); \
	$(CHECKOUT_LOCALES); \
	cvs_co $(CVSCO_SEAMONKEY);
	@echo "checkout finish: "`date` | tee -a $(CVSCO_LOGFILE)
# update the NSS checkout timestamp
	@if test `egrep -c '^(U|C) mozilla/security/(nss|coreconf)' $(CVSCO_LOGFILE) 2>/dev/null` != 0; then \
		touch $(TOPSRCDIR)/security/manager/.nss.checkout; \
	fi
#	@: Check the log for conflicts. ;
	@conflicts=`egrep "^C " $(CVSCO_LOGFILE)` ;\
	if test "$$conflicts" ; then \
	  echo "$(MAKE): *** Conflicts during checkout." ;\
	  echo "$$conflicts" ;\
	  echo "$(MAKE): Refer to $(CVSCO_LOGFILE) for full log." ;\
	  false; \
	else true; \
	fi
ifdef RUN_AUTOCONF_LOCALLY
	@echo Generating configures using $(AUTOCONF) ; \
	cd $(TOPSRCDIR) && $(AUTOCONF) && \
	cd $(TOPSRCDIR)/nsprpub && $(AUTOCONF) && \
	cd $(TOPSRCDIR)/directory/c-sdk && $(AUTOCONF)
endif

fast-update:
#	@: Backup the last checkout log.
	@if test -f $(CVSCO_LOGFILE) ; then \
	  mv $(CVSCO_LOGFILE) $(CVSCO_LOGFILE).old; \
	else true; \
	fi
ifdef RUN_AUTOCONF_LOCALLY
	@echo "Removing local configures" ; \
	cd $(ROOTDIR) && \
	$(RM) -f mozilla/configure mozilla/nsprpub/configure \
		mozilla/directory/c-sdk/configure
endif
	@echo "checkout start: "`date` | tee $(CVSCO_LOGFILE)
	@echo '$(CVSCO) mozilla/client.mk $(MOZCONFIG_MODULES)'; \
        cd $(ROOTDIR) && \
	$(CVSCO) mozilla/client.mk $(MOZCONFIG_MODULES)
	@cd $(TOPSRCDIR) && \
	$(MAKE) -f client.mk real_fast-update

# Start the update. Split the output to the tty and a log file.
real_fast-update:
	@set -e; \
	fast_update() { set -e; config/cvsco-fast-update.pl $$@ 2>&1 | tee -a $(CVSCO_LOGFILE); }; \
	cvs_co() { set -e; echo "$$@" ; \
	  "$$@" 2>&1 | tee -a $(CVSCO_LOGFILE); }; \
	fast_update $(CVSCO_NSPR); \
	cd $(ROOTDIR); \
	cvs_co $(CVSCO_NSS); \
	cd mozilla; \
	fast_update $(CVSCO_PSM); \
	fast_update $(CVSCO_LDAPCSDK); \
	fast_update $(CVSCO_ACCESSIBLE); \
	fast_update $(CVSCO_IMGLIB2); \
	fast_update $(CVSCO_IPC); \
	fast_update $(CVSCO_CALENDAR); \
	$(FASTUPDATE_LIBART); \
	$(FASTUPDATE_MOZTOOLKIT); \
	$(FASTUPDATE_PHOENIX); \
	$(FASTUPDATE_THUNDERBIRD); \
	$(FASTUPDATE_STANDALONE_COMPOSER); \
	$(FASTUPDATE_CODESIGHS); \
	$(FASTUPDATE_LOCALES); \
	fast_update $(CVSCO_SEAMONKEY);
	@echo "fast_update finish: "`date` | tee -a $(CVSCO_LOGFILE)
# update the NSS checkout timestamp
	@if test `egrep -c '^(U|C) mozilla/security/(nss|coreconf)' $(CVSCO_LOGFILE) 2>/dev/null` != 0; then \
		touch $(TOPSRCDIR)/security/manager/.nss.checkout; \
	fi
#	@: Check the log for conflicts. ;
	@conflicts=`egrep "^C " $(CVSCO_LOGFILE)` ;\
	if test "$$conflicts" ; then \
	  echo "$(MAKE): *** Conflicts during fast-update." ;\
	  echo "$$conflicts" ;\
	  echo "$(MAKE): Refer to $(CVSCO_LOGFILE) for full log." ;\
	  false; \
	else true; \
	fi
ifdef RUN_AUTOCONF_LOCALLY
	@echo Generating configures using $(AUTOCONF) ; \
	cd $(TOPSRCDIR) && $(AUTOCONF) && \
	cd $(TOPSRCDIR)/nsprpub && $(AUTOCONF) && \
	cd $(TOPSRCDIR)/directory/c-sdk && $(AUTOCONF)
endif

####################################
# Web configure

WEBCONFIG_FILE  := $(HOME)/.mozconfig

MOZCONFIG2CONFIGURATOR := build/autoconf/mozconfig2configurator
webconfig:
	@cd $(TOPSRCDIR); \
	url=`$(MOZCONFIG2CONFIGURATOR) $(TOPSRCDIR)`; \
	echo Running mozilla with the following url: ;\
	echo ;\
	echo $$url ;\
	mozilla -remote "openURL($$url)" || \
	netscape -remote "openURL($$url)" || \
	mozilla $$url || \
	netscape $$url ;\
	echo ;\
	echo   1. Fill out the form on the browser. ;\
	echo   2. Save the results to $(WEBCONFIG_FILE)

#####################################################
# First Checkout

ifdef _IS_FIRST_CHECKOUT
# First time, do build target in a new process to pick up new files.
build::
	$(MAKE) -f $(TOPSRCDIR)/client.mk build
else

#####################################################
# After First Checkout


####################################
# Configure

CONFIG_STATUS := $(wildcard $(OBJDIR)/config.status)
CONFIG_CACHE  := $(wildcard $(OBJDIR)/config.cache)

ifdef RUN_AUTOCONF_LOCALLY
EXTRA_CONFIG_DEPS := \
	$(TOPSRCDIR)/aclocal.m4 \
	$(wildcard $(TOPSRCDIR)/build/autoconf/*.m4) \
	$(NULL)

$(TOPSRCDIR)/configure: $(TOPSRCDIR)/configure.in $(EXTRA_CONFIG_DEPS)
	@echo Generating $@ using autoconf
	cd $(TOPSRCDIR); $(AUTOCONF)
endif

CONFIG_STATUS_DEPS_L10N := $(wildcard $(TOPSRCDIR)/l10n/makefiles.all)

CONFIG_STATUS_DEPS := \
	$(TOPSRCDIR)/configure \
	$(TOPSRCDIR)/allmakefiles.sh \
	$(TOPSRCDIR)/.mozconfig.mk \
	$(wildcard $(TOPSRCDIR)/nsprpub/configure) \
	$(wildcard $(TOPSRCDIR)/directory/c-sdk/configure) \
	$(wildcard $(TOPSRCDIR)/mailnews/makefiles) \
	$(CONFIG_STATUS_DEPS_L10N) \
	$(wildcard $(TOPSRCDIR)/themes/makefiles) \
	$(wildcard $(TOPSRCDIR)/config/milestone.txt) \
	$(wildcard $(TOPSRCDIR)/config/chrome-versions.sh) \
	$(NULL)

# configure uses the program name to determine @srcdir@. Calling it without
#   $(TOPSRCDIR) will set @srcdir@ to "."; otherwise, it is set to the full
#   path of $(TOPSRCDIR).
ifeq ($(TOPSRCDIR),$(OBJDIR))
  CONFIGURE := ./configure
else
  CONFIGURE := $(TOPSRCDIR)/configure
endif

ifdef MOZ_TOOLS
  CONFIGURE := $(TOPSRCDIR)/configure
endif

$(OBJDIR)/Makefile $(OBJDIR)/config.status: $(CONFIG_STATUS_DEPS)
	@if test ! -d $(OBJDIR); then $(MKDIR) $(OBJDIR); else true; fi
	@echo cd $(OBJDIR);
	@echo $(CONFIGURE) $(CONFIGURE_ARGS)
	@cd $(OBJDIR) && $(CONFIGURE_ENV_ARGS) $(CONFIGURE) $(CONFIGURE_ARGS) \
	  || ( echo "*** Fix above errors and then restart with\
               \"$(MAKE) -f client.mk build\"" && exit 1 )
	@touch $(OBJDIR)/Makefile

ifdef CONFIG_STATUS
$(OBJDIR)/config/autoconf.mk: $(TOPSRCDIR)/config/autoconf.mk.in
	cd $(OBJDIR); \
	  CONFIG_FILES=config/autoconf.mk ./config.status
endif


####################################
# Depend

depend:: $(OBJDIR)/Makefile $(OBJDIR)/config.status
	$(MOZ_MAKE) export && $(MOZ_MAKE) depend

####################################
# Build it

build::  $(OBJDIR)/Makefile $(OBJDIR)/config.status
	$(MOZ_MAKE)

####################################
# Profile-feedback build (gcc only)
#  To use this, you should set the following variables in your mozconfig
#    mk_add_options PROFILE_GEN_SCRIPT=/path/to/profile-script
#
#  The profile script should exercise the functionality to be included
#  in the profile feedback.

profiledbuild:: $(OBJDIR)/Makefile $(OBJDIR)/config.status
	$(MOZ_MAKE) MOZ_PROFILE_GENERATE=1
	OBJDIR=${OBJDIR} $(PROFILE_GEN_SCRIPT)
	$(MOZ_MAKE) clobber_all
	$(MOZ_MAKE) MOZ_PROFILE_USE=1
	find $(OBJDIR) -name "*.da" -exec rm {} \;

####################################
# Other targets

# Pass these target onto the real build system
install export libs clean realclean distclean alldep:: $(OBJDIR)/Makefile $(OBJDIR)/config.status
	$(MOZ_MAKE) $@

cleansrcdir:
	@cd $(TOPSRCDIR); \
        if [ -f webshell/embed/gtk/Makefile ]; then \
          $(MAKE) -C webshell/embed/gtk distclean; \
        fi; \
	if [ -f Makefile ]; then \
	  $(MAKE) distclean ; \
	else \
	  echo "Removing object files from srcdir..."; \
	  rm -fr `find . -type d \( -name .deps -print -o -name CVS \
	          -o -exec test ! -d {}/CVS \; \) -prune \
	          -o \( -name '*.[ao]' -o -name '*.so' \) -type f -print`; \
	   build/autoconf/clean-config.sh; \
	fi;

# (! IS_FIRST_CHECKOUT)
endif

echo_objdir:
	@echo $(OBJDIR)

.PHONY: checkout real_checkout depend build export libs alldep install clean realclean distclean cleansrcdir pull_all build_all clobber clobber_all pull_and_build_all everything
