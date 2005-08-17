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
#   Benjamin Smedberg <bsmedberg@covad.net>
#   Chase Phillips <chase@mozilla.org>
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

# Build a mozilla application.
#
# To checkout and build a tree,
#    1. cvs co mozilla/client.mk
#    2. cd mozilla
#    3. create your .mozconfig file with
#       mk_add_options MOZ_CO_PROJECT=suite,browser,mail,minimo,xulrunner
#    4. gmake -f client.mk 
#
# This script will pick up the CVSROOT from the CVS/Root file. If you wish
# to use a different CVSROOT, you must set CVSROOT in your environment:
#
#   export CVSROOT=:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
#   export CVSROOT=:pserver:username%somedomain.org@cvs.mozilla.org:/cvsroot
# 
# You must specify which products/modules you wish to checkout, with
#   MOZ_CO_PROJECT, MOZ_CO_MODULE, and BUILD_MODULES variables.
#
#   MOZ_CO_PROJECT possibilities include the following:
#     suite (Seamonkey suite)
#     browser (aka Firefox)
#     mail (aka Thunderbird)
#     minimo (small browser for devices)
#     composer (standalone composer, aka NVU)
#     calendar (aka Sunbird, use this to build the calendar extensions also)
#     xulrunner
#     macbrowser (aka Camino)
#
# Other common MOZ_CO_MODULE options include the following:
#   mozilla/other-licenses/libart_lgpl
#   mozilla/other-licenses/bsdiff
#   mozilla/tools/codesighs
#
# Other targets (gmake -f client.mk [targets...]),
#    checkout
#    build
#    clean (realclean is now the same as clean)
#    distclean
#
# See http://www.mozilla.org/build/ for more information.
#
# Options:
#   MOZ_OBJDIR           - Destination object directory
#   MOZ_CO_DATE          - Date tag to use for checkout (default: none)
#   MOZ_CO_MODULE        - Module to checkout
#   MOZ_CVS_FLAGS        - Flags to pass cvs (default: -q -z3)
#   MOZ_CO_FLAGS         - Flags to pass after 'cvs co' (default: -P)
#   MOZ_MAKE_FLAGS       - Flags to pass to $(MAKE)
#   MOZ_CO_LOCALES       - localizations to pull (MOZ_CO_LOCALES="de-DE,pt-BR")
#   MOZ_LOCALE_DIRS      - directories which contain localizations
#   LOCALES_CVSROOT      - CVSROOT to use to pull localizations
#

AVAILABLE_PROJECTS = \
  all \
  suite \
  toolkit \
  browser \
  mail \
  minimo \
  composer \
  calendar \
  xulrunner \
  macbrowser \
  $(NULL)

MODULES_core :=                                 \
  SeaMonkeyAll                                  \
  browser/config/version.txt                    \
  mail/config/version.txt                       \
  mozilla/ipc/ipcd                              \
  mozilla/modules/libpr0n                       \
  mozilla/modules/libmar                        \
  mozilla/modules/libbz2                        \
  mozilla/accessible                            \
  mozilla/security/manager                      \
  mozilla/toolkit                               \
  mozilla/storage                               \
  mozilla/db/sqlite3                            \
  $(NULL)

LOCALES_core :=                                 \
  netwerk                                       \
  dom                                           \
  $(NULL)

MODULES_toolkit :=                              \
  $(MODULES_core)                               \
  mozilla/chrome                                \
  $(NULL)

LOCALES_toolkit :=                              \
  $(LOCALES_core)                               \
  toolkit                                       \
  security/manager                              \
  $(NULL)

MODULES_suite :=                                \
  $(MODULES_core)                               \
  mozilla/suite                                 \
  $(NULL)

LOCALES_suite :=                                \
  $(LOCALES_core)                               \
  $(NULL)

MODULES_browser :=                              \
  $(MODULES_toolkit)                            \
  mozilla/browser                               \
  mozilla/other-licenses/branding/firefox       \
  mozilla/other-licenses/7zstub/firefox         \
  $(NULL)

MODULES_minimo :=                               \
  $(MODULES_toolkit)                            \
  $(NULL)

LOCALES_browser :=                              \
  $(LOCALES_toolkit)                            \
  browser                                       \
  other-licenses/branding/firefox               \
  $(NULL)

BOOTSTRAP_browser := mozilla/browser/config/mozconfig

BOOTSTRAP_minimo := mozilla/minimo

MODULES_mail :=                                 \
  $(MODULES_toolkit)                            \
  mozilla/mail                                  \
  mozilla/other-licenses/branding/thunderbird   \
  mozilla/other-licenses/7zstub/thunderbird     \
  $(NULL)

LOCALES_mail :=                                 \
  $(LOCALES_toolkit)                            \
  mail                                          \
  other-licenses/branding/thunderbird           \
  editor/ui                                     \
  $(NULL)

BOOTSTRAP_mail := mozilla/mail/config/mozconfig

MODULES_composer :=                             \
  $(MODULES_toolkit)                            \
  mozilla/composer                              \
  $(NULL)

MODULES_calendar :=                             \
  $(MODULES_toolkit)                            \
  mozilla/storage                               \
  mozilla/db/sqlite3                            \
  mozilla/calendar                              \
  $(NULL)

BOOTSTRAP_calendar := mozilla/calendar/sunbird/config/mozconfig

MODULES_xulrunner :=                            \
  $(MODULES_toolkit)                            \
  mozilla/xulrunner                             \
  $(NULL)

LOCALES_xulrunner :=                            \
  $(LOCALES_toolkit)                            \
  $(NULL)

BOOTSTRAP_xulrunner := mozilla/xulrunner/config/mozconfig

MODULES_macbrowser :=                           \
  $(MODULES_core)                               \
  mozilla/camino                                \
  $(NULL)

BOOTSTRAP_macbrowser := mozilla/camino/config/mozconfig

MODULES_all :=                                  \
  mozilla/other-licenses/bsdiff                 \
  mozilla/other-licenses/libart_lgpl            \
  mozilla/tools/trace-malloc                    \
  mozilla/tools/jprof                           \
  mozilla/tools/codesighs                       \
  mozilla/tools/update-packaging                \
  mozilla/other-licenses/branding               \
  mozilla/other-licenses/7zstub                 \
  $(NULL)

#######################################################################
# Checkout Tags
#
# For branches, uncomment the MOZ_CO_TAG line with the proper tag,
# and commit this file on that tag.
#MOZ_CO_TAG          = <tag>
NSPR_CO_TAG          = NSPRPUB_PRE_4_2_CLIENT_BRANCH
NSS_CO_TAG           = NSS_CLIENT_TAG
LDAPCSDK_CO_TAG      = ldapcsdk_50_client_branch
LOCALES_CO_TAG       =

BUILD_MODULES = all

#######################################################################
# Defines
#
CVS = cvs
comma := ,

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

CVS_CO_DATE_FLAGS = $(if $(MOZ_CO_DATE),-D "$(MOZ_CO_DATE)")
CVSCO = $(CVS) $(CVS_FLAGS) co $(MOZ_CO_FLAGS) $(if $(MOZ_CO_TAG),-r $(MOZ_CO_TAG)) $(CVS_CO_DATE_FLAGS)

CVSCO_LOGFILE := $(ROOTDIR)/cvsco.log
CVSCO_LOGFILE := $(shell echo $(CVSCO_LOGFILE) | sed s%//%/%)

# if LOCALES_CVSROOT is not specified, set it here
# (and let mozconfig override it)
LOCALES_CVSROOT ?= :pserver:anonymous@cvs-mirror.mozilla.org:/l10n

####################################
# Load mozconfig Options

# See build pages, http://www.mozilla.org/build/ for how to set up mozconfig.

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

MOZ_PROJECT_LIST := $(subst $(comma), ,$(MOZ_CO_PROJECT))

ifneq (,$(filter-out $(AVAILABLE_PROJECTS),$(MOZ_PROJECT_LIST)))
$(error MOZ_CO_PROJECT contains an unrecognized project.)
endif

ifeq (all,$(filter all,$(MOZ_PROJECT_LIST)))
  MOZ_PROJECT_LIST := $(AVAILABLE_PROJECTS)
endif

MOZ_MODULE_LIST := $(subst $(comma), ,$(MOZ_CO_MODULE)) $(foreach project,$(MOZ_PROJECT_LIST),$(MODULES_$(project)))
LOCALE_DIRS := $(MOZ_LOCALE_DIRS) $(foreach project,$(MOZ_PROJECT_LIST),$(LOCALES_$(project)))

MOZCONFIG_MODULES += $(foreach project,$(MOZ_PROJECT_LIST),$(BOOTSTRAP_$(project)))

# Using $(sort) here because it also removes duplicate entries.
MOZ_MODULE_LIST := $(sort $(MOZ_MODULE_LIST))
LOCALE_DIRS := $(sort $(LOCALE_DIRS))
MOZCONFIG_MODULES := $(sort $(MOZCONFIG_MODULES))

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

ifdef MOZ_OBJDIR
  OBJDIR := $(MOZ_OBJDIR)
  MOZ_MAKE := $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)
else
  OBJDIR := $(TOPSRCDIR)
  MOZ_MAKE := $(MAKE) $(MOZ_MAKE_FLAGS)
endif

####################################
# CVS defines for NSS
#
NSS_CO_MODULE =               \
		mozilla/security/nss      \
		mozilla/security/coreconf \
		$(NULL)

NSS_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  NSS_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
NSS_CO_FLAGS := $(NSS_CO_FLAGS) $(if $(NSS_CO_TAG),-r $(NSS_CO_TAG),-A)

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
NSPR_CO_FLAGS := $(NSPR_CO_FLAGS) $(if $(NSPR_CO_TAG),-r $(NSPR_CO_TAG),-A)

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
LDAPCSDK_CO_FLAGS := $(LDAPCSDK_CO_FLAGS) $(if $(LDAPCSDK_CO_TAG),-r $(LDAPCSDK_CO_TAG),-A)
CVSCO_LDAPCSDK = $(CVS) $(CVS_FLAGS) co $(LDAPCSDK_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(LDAPCSDK_CO_MODULE)

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

	MOZ_MODULE_LIST += $(addprefix mozilla/,$(STANDALONE_CO_MODULE))
  NOSUBDIRS_MODULE := $(addprefix mozilla/,$(BUILD_MODULE_CVS_NS))

ifeq (,$(filter $(NSPRPUB_DIR), $(BUILD_MODULE_CVS))$(MOZ_CO_PROJECT))
  CVSCO_NSPR :=
endif
ifeq (,$(filter security security/manager, $(BUILD_MODULE_CVS))$(MOZ_CO_PROJECT))
  CVSCO_NSS :=
endif
ifeq (,$(filter directory/c-sdk, $(BUILD_MODULE_CVS))$(MOZ_CO_PROJECT))
  CVSCO_LDAPCSDK :=
endif
endif

####################################
# Error on obsolete variables.
#

ifdef MOZ_MAPINFO
$(warning MOZ_MAPINFO is obsolete, use MOZ_CO_MODULE=mozilla/tools/codesighs instead.)
MOZ_MODULE_LIST += mozilla/tools/codesighs
endif
ifdef MOZ_INTERNAL_LIBART_LGPL
$(error MOZ_INTERNAL_LIBART_LGPL is obsolete, use MOZ_CO_MODULE=mozilla/other-licenses/libart_lgpl instead.)
endif
ifdef MOZ_PHOENIX
$(warning MOZ_PHOENIX is obsolete.)
MOZ_MODULE_LIST += $(MODULES_browser)
# $(error MOZ_PHOENIX is obsolete, use MOZ_CO_PROJECT=browser and --enable-application=browser)
endif
ifdef MOZ_THUNDERBIRD
$(warning MOZ_THUNDERBIRD is obsolete.)
MOZ_MODULE_LIST += $(MODULES_mail)
# $(error MOZ_THUNDERBIRD is obsolete, use MOZ_CO_PROJECT=mail and --enable-application=mail)
endif

###################################
# Checkout main modules
#

# sort is used to remove duplicates.  SeaMonkeyAll is special-cased to
# checkout last, because if you check it out first, there is a sticky
# tag left over from checking out the LDAP SDK, which causes files in
# the root directory to be missed.
MOZ_MODULE_LIST := $(sort $(filter-out SeaMonkeyAll,$(MOZ_MODULE_LIST))) $(firstword $(filter SeaMonkeyAll,$(MOZ_MODULE_LIST)))

MODULES_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  MODULES_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
MODULES_CO_FLAGS := $(MODULES_CO_FLAGS) $(if $(MOZ_CO_TAG),-r $(MOZ_CO_TAG),-A)

CVSCO_MODULES_NS = $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) -l $(NOSUBDIRS_MODULE)

ifeq (,$(strip $(MOZ_MODULE_LIST)))
FASTUPDATE_MODULES = $(error No modules or projects were specified. Use MOZ_CO_PROJECT to specify a project for checkout.)
CHECKOUT_MODULES   = $(error No modules or projects were specified. Use MOZ_CO_PROJECT to specify a project for checkout.)
else
FASTUPDATE_MODULES := fast_update $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(MOZ_MODULE_LIST)
CHECKOUT_MODULES   := $(foreach module,$(MOZ_MODULE_LIST),cvs_co $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(module);)
endif
ifeq (,$(NOSUBDIRS_MODULE))
FASTUPDATE_MODULES_NS := true
CHECKOUT_MODULES_NS   := true
else
FASTUPDATE_MODULES_NS := fast_update $(CVSCO_MODULES_NS)
CHECKOUT_MODULES_NS   := cvs_co      $(CVSCO_MODULES_NS)
endif

###################################
# CVS defines for locales
#

LOCALES_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  LOCALES_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
LOCALES_CO_FLAGS := $(LOCALES_CO_FLAGS) $(if $(LOCALES_CO_TAG),-r $(LOCALES_CO_TAG),-A)

ifndef MOZ_CO_LOCALES
FASTUPDATE_LOCALES := true
CHECKOUT_LOCALES := true
else

override MOZ_CO_LOCALES := $(subst $(comma), ,$(MOZ_CO_LOCALES))

ifeq (all,$(MOZ_CO_LOCALES))
MOZCONFIG_MODULES += $(foreach project,$(MOZ_PROJECT_LIST),mozilla/$(project)/locales/all-locales)

LOCALE_CO_DIRS := $(sort $(foreach project,$(MOZ_PROJECT_LIST),$(foreach locale,$(shell cat mozilla/$(project)/locales/all-locales),$(foreach dir,$(LOCALES_$(project)),l10n/$(locale)/$(dir)))))
else # MOZ_CO_LOCALES != all
LOCALE_CO_DIRS = $(sort $(foreach locale,$(MOZ_CO_LOCALES),$(foreach dir,$(LOCALE_DIRS),l10n/$(locale)/$(dir))))
endif

CVSCO_LOCALES := $(CVS) $(CVS_FLAGS) -d $(LOCALES_CVSROOT) co $(LOCALES_CO_FLAGS) $(LOCALE_CO_DIRS)

FASTUPDATE_LOCALES := fast_update $(CVSCO_LOCALES)
CHECKOUT_LOCALES := cvs_co $(CVSCO_LOCALES)
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
	cvs_co $(CVSCO_NSPR); \
	cvs_co $(CVSCO_NSS); \
	cvs_co $(CVSCO_LDAPCSDK); \
	$(CHECKOUT_MODULES) \
	$(CHECKOUT_MODULES_NS); \
	$(CHECKOUT_LOCALES);
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
	fast_update $(CVSCO_LDAPCSDK); \
	$(FASTUPDATE_MODULES); \
	$(FASTUPDATE_MODULES_NS); \
	$(FASTUPDATE_LOCALES);
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

CONFIG_STATUS_DEPS := \
	$(TOPSRCDIR)/configure \
	$(TOPSRCDIR)/allmakefiles.sh \
	$(TOPSRCDIR)/.mozconfig.mk \
	$(wildcard $(TOPSRCDIR)/nsprpub/configure) \
	$(wildcard $(TOPSRCDIR)/directory/c-sdk/configure) \
	$(wildcard $(TOPSRCDIR)/mailnews/makefiles) \
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

configure:
	@if test ! -d $(OBJDIR); then $(MKDIR) $(OBJDIR); else true; fi
	@echo cd $(OBJDIR);
	@echo $(CONFIGURE) $(CONFIGURE_ARGS)
	@cd $(OBJDIR) && $(CONFIGURE_ENV_ARGS) $(CONFIGURE) $(CONFIGURE_ARGS) \
	  || ( echo "*** Fix above errors and then restart with\
               \"$(MAKE) -f client.mk build\"" && exit 1 )
	@touch $(OBJDIR)/Makefile

$(OBJDIR)/Makefile $(OBJDIR)/config.status: $(CONFIG_STATUS_DEPS)
	@$(MAKE) -f $(TOPSRCDIR)/client.mk configure

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

.PHONY: checkout real_checkout depend build export libs alldep install clean realclean distclean cleansrcdir pull_all build_all clobber clobber_all pull_and_build_all everything configure
