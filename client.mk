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
#   Mark Mentovai <mark@moxienet.com>
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
#       mk_add_options MOZ_CO_PROJECT=
#         suite,browser
#    4. gmake -f client.mk 
#
# This script will pick up the CVSROOT from the CVS/Root file. If you wish
# to use a different CVSROOT, you must set CVSROOT in your environment:
#
#   export CVSROOT=:pserver:anonymous:anonymous@cvs-mirror.mozilla.org:/cvsroot
#   export CVSROOT=:pserver:username%somedomain.org@cvs.mozilla.org:/cvsroot
# 
# You must specify which products/modules you wish to checkout, with
#   MOZ_CO_PROJECT and MOZ_CO_MODULE variables.
#
#   MOZ_CO_PROJECT possibilities include the following:
#     suite (Seamonkey suite)
#     browser (aka Firefox)
#     mail (aka Thunderbird)
#     minimo (small browser for devices)
#     composer (standalone composer, aka NVU)
#     calendar (aka Sunbird, use this to build the calendar extensions also)
#     xulrunner
#     camino
#     tamarin
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
#   MOZ_BUILD_PROJECTS   - Build multiple projects in subdirectories
#                          of MOZ_OBJDIR
#   MOZ_OBJDIR           - Destination object directory
#   MOZ_CO_DATE          - Date tag to use for checkout (default: none)
#   MOZ_CO_LOCALES_DATE  - Date tag to use for locale checkout
#                          (default: MOZ_CO_DATE)
#   MOZ_CO_MODULE        - Module to checkout
#   MOZ_CVS_FLAGS        - Flags to pass cvs (default: -q -z3)
#   MOZ_CO_FLAGS         - Flags to pass after 'cvs co' (default: -P)
#   MOZ_MAKE_FLAGS       - Flags to pass to $(MAKE)
#   MOZ_CO_LOCALES       - localizations to pull (MOZ_CO_LOCALES="de-DE,pt-BR")
#   MOZ_LOCALE_DIRS      - directories which contain localizations
#   LOCALES_CVSROOT      - CVSROOT to use to pull localizations
#   MOZ_PREFLIGHT_ALL  } - Makefiles to run before any project in
#   MOZ_PREFLIGHT      }   MOZ_BUILD_PROJECTS, before each project, after
#   MOZ_POSTFLIGHT     }   each project, and after all projects; these
#   MOZ_POSTFLIGHT_ALL }   variables contain space-separated lists
#   MOZ_UNIFY_BDATE      - Set to use the same bdate for each project in
#                          MOZ_BUILD_PROJECTS
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
  camino \
  necko \
  tamarin \
  $(NULL)

# Trailing / on top-level mozilla dir required to stop fast-update thinking
# it is a module name.
MODULES_NS_necko :=                             \
  mozilla/                                      \
  $(NULL)

MODULES_necko :=                                \
  mozilla/README                                \
  mozilla/config                                \
  mozilla/build                                 \
  mozilla/intl                                  \
  mozilla/modules/libpref                       \
  mozilla/modules/zlib                          \
  mozilla/netwerk                               \
  mozilla/xpcom                                 \
  mozilla/tools/test-harness                    \
  $(NULL)

MODULES_NS_core :=                              \
  $(MODULES_NS_necko)                           \
  mozilla/js                                    \
  mozilla/js/src                                \
  mozilla/js/jsd                                \
  mozilla/db                                    \
  $(NULL)

MODULES_core :=                                 \
  $(MODULES_necko)                              \
  mozilla/caps                                  \
  mozilla/content                               \
  mozilla/db/mdb                                \
  mozilla/db/mork                               \
  mozilla/docshell                              \
  mozilla/dom                                   \
  mozilla/editor                                \
  mozilla/embedding                             \
  mozilla/extensions                            \
  mozilla/gfx                                   \
  mozilla/parser                                \
  mozilla/layout                                \
  mozilla/jpeg                                  \
  mozilla/js/src/fdlibm                         \
  mozilla/js/src/liveconnect                    \
  mozilla/js/src/xpconnect                      \
  mozilla/js/jsd/idl                            \
  mozilla/modules/libimg                        \
  mozilla/modules/libjar                        \
  mozilla/modules/libpr0n                       \
  mozilla/modules/libreg                        \
  mozilla/modules/libutil                       \
  mozilla/modules/oji                           \
  mozilla/modules/plugin                        \
  mozilla/modules/staticmod                     \
  mozilla/plugin/oji                            \
  mozilla/profile                               \
  mozilla/rdf                                   \
  mozilla/security/manager                      \
  mozilla/sun-java                              \
  mozilla/ipc/ipcd                              \
  mozilla/modules/libpr0n                       \
  mozilla/modules/libmar                        \
  mozilla/modules/libbz2                        \
  mozilla/accessible                            \
  mozilla/other-licenses/atk-1.0                \
  mozilla/other-licenses/ia2                    \
  mozilla/security/manager                      \
  mozilla/tools/elf-dynstr-gc                   \
  mozilla/uriloader                             \
  mozilla/view                                  \
  mozilla/webshell                              \
  mozilla/widget                                \
  mozilla/xpfe                                  \
  mozilla/xpinstall                             \
  mozilla/toolkit                               \
  mozilla/storage                               \
  mozilla/db/sqlite3                            \
  mozilla/db/morkreader                         \
  mozilla/testing/mochitest                     \
  $(NULL)

LOCALES_necko :=                                \
  netwerk                                       \
  $(NULL)

LOCALES_core :=                                 \
  $(LOCALES_necko)                              \
  dom                                           \
  $(NULL)

BOOTSTRAP_necko :=                              \
  mozilla/browser/config/version.txt            \
  mozilla/mail/config/version.txt               \
  mozilla/calendar/sunbird/config/version.txt   \
  mozilla/suite/config/version.txt              \
  $(NULL)

BOOTSTRAP_core :=                               \
  $(BOOTSTRAP_necko)                            \
  $(NULL)

MODULES_NS_toolkit :=                           \
  $(MODULES_NS_core)                            \
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

BOOTSTRAP_toolkit :=                            \
  $(BOOTSTRAP_core)                             \
  $(NULL)

MODULES_NS_suite :=                             \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_suite :=                                \
  $(MODULES_toolkit)                            \
  mozilla/directory/xpcom                       \
  mozilla/mailnews                              \
  mozilla/themes                                \
  mozilla/suite                                 \
  $(NULL)

LOCALES_suite :=                                \
  $(LOCALES_toolkit)                            \
  suite                                         \
  editor/ui                                     \
  extensions/reporter                           \
  $(NULL)

BOOTSTRAP_suite :=                              \
  $(BOOTSTRAP_toolkit)                          \
  $(NULL)

MODULES_NS_browser :=                           \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_browser :=                              \
  $(MODULES_toolkit)                            \
  mozilla/browser                               \
  mozilla/other-licenses/branding/firefox       \
  mozilla/other-licenses/7zstub/firefox         \
  $(NULL)

LOCALES_browser :=                              \
  $(LOCALES_toolkit)                            \
  browser                                       \
  extensions/reporter                           \
  extensions/spellcheck                         \
  other-licenses/branding/firefox               \
  $(NULL)

BOOTSTRAP_browser :=                            \
  $(BOOTSTRAP_toolkit)                          \
  mozilla/browser/config/mozconfig              \
  $(NULL)

MODULES_NS_minimo :=                            \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_minimo :=                               \
  $(MODULES_toolkit)                            \
  mozilla/minimo                                \
  $(NULL)

BOOTSTRAP_minimo :=                             \
  $(BOOTSTRAP_toolkit)                          \
  $(NULL)

MODULES_NS_mail :=                              \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_mail :=                                 \
  $(MODULES_toolkit)                            \
  mozilla/directory/xpcom                       \
  mozilla/mailnews                              \
  mozilla/mail                                  \
  mozilla/other-licenses/branding/thunderbird   \
  mozilla/other-licenses/7zstub/thunderbird     \
  $(NULL)

LOCALES_mail :=                                 \
  $(LOCALES_toolkit)                            \
  mail                                          \
  other-licenses/branding/thunderbird           \
  editor/ui                                     \
  extensions/spellcheck                         \
  $(NULL)

BOOTSTRAP_mail :=                               \
  $(BOOTSTRAP_toolkit)                          \
  mozilla/mail/config/mozconfig                 \
  $(NULL)

MODULES_composer :=                             \
  $(MODULES_toolkit)                            \
  mozilla/composer                              \
  $(NULL)

MODULES_NS_calendar :=                          \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_calendar :=                             \
  $(MODULES_toolkit)                            \
  mozilla/storage                               \
  mozilla/db/sqlite3                            \
  mozilla/calendar                              \
  mozilla/other-licenses/branding/sunbird       \
  mozilla/other-licenses/7zstub/sunbird         \
  $(NULL)

LOCALES_calendar :=                             \
  $(LOCALES_toolkit)                            \
  calendar                                      \
  other-licenses/branding/sunbird               \
  $(NULL)

BOOTSTRAP_calendar :=                           \
  $(BOOTSTRAP_toolkit)                          \
  mozilla/calendar/sunbird/config/mozconfig     \
  $(NULL)

MODULES_NS_xulrunner :=                         \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_xulrunner :=                            \
  $(MODULES_toolkit)                            \
  mozilla/xulrunner                             \
  $(NULL)

LOCALES_xulrunner :=                            \
  $(LOCALES_toolkit)                            \
  $(NULL)

BOOTSTRAP_xulrunner :=                          \
  $(BOOTSTRAP_toolkit)                          \
  mozilla/xulrunner/config/mozconfig            \
  $(NULL)

MODULES_NS_camino :=                            \
  $(MODULES_NS_toolkit)                         \
  $(NULL)

MODULES_camino :=                               \
  $(MODULES_core)                               \
  mozilla/camino                                \
  mozilla/themes                                \
  $(NULL)

BOOTSTRAP_camino :=                             \
  $(BOOTSTRAP_toolkit)                          \
  mozilla/camino/config/mozconfig               \
  $(NULL)

MODULES_tamarin :=                              \
  mozilla/js/tamarin                            \
  mozilla/modules/zlib                          \
  $(NULL)

MODULES_all :=                                  \
  mozilla/other-licenses/bsdiff                 \
  mozilla/other-licenses/libart_lgpl            \
  mozilla/tools/trace-malloc                    \
  mozilla/tools/jprof                           \
  mozilla/tools/codesighs                       \
  mozilla/tools/update-packaging                \
  $(NULL)

#######################################################################
# Checkout Tags
#
# For branches, uncomment the MOZ_CO_TAG line with the proper tag,
# and commit this file on that tag.
#MOZ_CO_TAG          = <tag>
NSPR_CO_TAG          = NSPRPUB_PRE_4_2_CLIENT_BRANCH
NSS_CO_TAG           = NSS_3_11_5_RTM
LDAPCSDK_CO_TAG      = ldapcsdk_5_17_client_branch
LOCALES_CO_TAG       =

#######################################################################
# Defines
#
CVS = cvs
comma := ,

CWD := $(shell pwd)
ifneq (1,$(words $(CWD)))
$(error The mozilla directory cannot be located in a path with spaces.)
endif

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
PERL ?= perl

CONFIG_GUESS_SCRIPT := $(wildcard $(TOPSRCDIR)/build/autoconf/config.guess)
ifdef CONFIG_GUESS_SCRIPT
  CONFIG_GUESS = $(shell $(CONFIG_GUESS_SCRIPT))
else
  _IS_FIRST_CHECKOUT := 1
endif

####################################
# Sanity checks

ifneq (,$(filter MINGW%,$(shell uname -s)))
# check for CRLF line endings
ifneq (0,$(shell $(PERL) -e 'binmode(STDIN); while (<STDIN>) { if (/\r/) { print "1"; exit } } print "0"' < $(TOPSRCDIR)/client.mk))
$(error This source tree appears to have Windows-style line endings. To \
convert it to Unix-style line endings, run \
"python mozilla/build/win32/mozilla-dos2unix.py")
endif
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
CVS_CO_LOCALES_DATE_FLAGS = $(if $(MOZ_CO_LOCALES_DATE),-D "$(MOZ_CO_LOCALES_DATE)")
CVSCO = $(CVS) $(CVS_FLAGS) co $(MOZ_CO_FLAGS) $(if $(MOZ_CO_TAG),-r $(MOZ_CO_TAG)) $(CVS_CO_DATE_FLAGS)

MOZ_CO_LOCALES_DATE ?= $(MOZ_CO_DATE)

CVSCO_LOGFILE := $(ROOTDIR)/cvsco.log
CVSCO_LOGFILE := $(shell echo $(CVSCO_LOGFILE) | sed s%//%/%)

# if LOCALES_CVSROOT is not specified, set it here
# (and let mozconfig override it)
LOCALES_CVSROOT ?= :pserver:anonymous:anonymous@cvs-mirror.mozilla.org:/l10n

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
MOZ_PROJECT_LIST := $(subst macbrowser,camino,$(MOZ_PROJECT_LIST))

ifneq (,$(filter-out $(AVAILABLE_PROJECTS),$(MOZ_PROJECT_LIST)))
$(error MOZ_CO_PROJECT contains an unrecognized project.)
endif

ifeq (all,$(filter all,$(MOZ_PROJECT_LIST)))
  MOZ_PROJECT_LIST := $(AVAILABLE_PROJECTS)
endif

MOZ_MODULE_LIST := $(subst $(comma), ,$(MOZ_CO_MODULE)) $(foreach project,$(MOZ_PROJECT_LIST),$(MODULES_$(project)))
MOZ_MODULE_LIST_NS := $(foreach project,$(MOZ_PROJECT_LIST),$(MODULES_NS_$(project)))
LOCALE_DIRS := $(MOZ_LOCALE_DIRS) $(foreach project,$(MOZ_PROJECT_LIST),$(LOCALES_$(project)))

MOZCONFIG_MODULES += $(foreach project,$(MOZ_PROJECT_LIST),$(BOOTSTRAP_$(project)))

# Using $(sort) here because it also removes duplicate entries.
MOZ_MODULE_LIST := $(sort $(MOZ_MODULE_LIST))
LOCALE_DIRS := $(sort $(LOCALE_DIRS))
MOZCONFIG_MODULES := $(sort $(MOZCONFIG_MODULES))

# Change CVS flags if anonymous root is requested
ifdef MOZ_CO_USE_MIRROR
  CVS_FLAGS := -d :pserver:anonymous:anonymous@cvs-mirror.mozilla.org:/cvsroot
endif

# MOZ_CVS_FLAGS - Basic CVS flags
ifeq "$(origin MOZ_CVS_FLAGS)" "undefined"
  CVS_FLAGS := $(CVS_FLAGS) -q -z 3 
else
  CVS_FLAGS := $(MOZ_CVS_FLAGS)
endif

ifdef MOZ_BUILD_PROJECTS

ifndef MOZ_OBJDIR
  $(error When MOZ_BUILD_PROJECTS is set, you must set MOZ_OBJDIR)
endif
ifdef MOZ_CURRENT_PROJECT
  OBJDIR = $(MOZ_OBJDIR)/$(MOZ_CURRENT_PROJECT)
  MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)
  BUILD_PROJECT_ARG = MOZ_BUILD_APP=$(MOZ_CURRENT_PROJECT)
else
  OBJDIR = $(error Cannot find the OBJDIR when MOZ_CURRENT_PROJECT is not set.)
  MOZ_MAKE = $(error Cannot build in the OBJDIR when MOZ_CURRENT_PROJECT is not set.)
endif

else # MOZ_BUILD_PROJECTS

ifdef MOZ_OBJDIR
  OBJDIR = $(MOZ_OBJDIR)
  MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)
else
  OBJDIR := $(TOPSRCDIR)
  MOZ_MAKE := $(MAKE) $(MOZ_MAKE_FLAGS)
endif

endif # MOZ_BUILD_PROJECTS

####################################
# CVS defines for NSS
#
NSS_CO_MODULE =               \
		mozilla/dbm               \
		mozilla/security/nss      \
		mozilla/security/coreconf \
		mozilla/security/dbm      \
		$(NULL)

NSS_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  NSS_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
NSS_CO_FLAGS := $(NSS_CO_FLAGS) $(if $(NSS_CO_TAG),-r $(NSS_CO_TAG),-A)

# Can only pull the tip or branch tags by date
ifeq (,$(filter-out HEAD %BRANCH,$(NSS_CO_TAG)))
CVSCO_NSS = $(CVS) $(CVS_FLAGS) co $(NSS_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(NSS_CO_MODULE)
else
CVSCO_NSS = $(CVS) $(CVS_FLAGS) co $(NSS_CO_FLAGS) $(NSS_CO_MODULE)
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

# Can only pull the tip or branch tags by date
ifeq (,$(filter-out HEAD %BRANCH,$(NSPR_CO_TAG)))
CVSCO_NSPR = $(CVS) $(CVS_FLAGS) co $(NSPR_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(NSPR_CO_MODULE)
else
CVSCO_NSPR = $(CVS) $(CVS_FLAGS) co $(NSPR_CO_FLAGS) $(NSPR_CO_MODULE)
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

# sort is used to remove duplicates.
MOZ_MODULE_LIST := $(sort $(MOZ_MODULE_LIST))
MOZ_MODULE_LIST_NS := $(sort $(MOZ_MODULE_LIST_NS))

####################################
# Suppress standalone modules if they're not needed.
#
ifeq (,$(filter mozilla/xpcom,$(MOZ_MODULE_LIST)))
  CVSCO_NSPR :=
endif

ifeq (,$(filter mozilla/security/manager,$(MOZ_MODULE_LIST)))
  CVSCO_NSS :=
endif
ifeq (,$(filter mozilla/directory/xpcom,$(MOZ_MODULE_LIST)))
  CVSCO_LDAPCSDK :=
endif

MODULES_CO_FLAGS := -P
ifdef MOZ_CO_FLAGS
  MODULES_CO_FLAGS := $(MOZ_CO_FLAGS)
endif
MODULES_CO_FLAGS := $(MODULES_CO_FLAGS) $(if $(MOZ_CO_TAG),-r $(MOZ_CO_TAG),-A)

CVSCO_MODULES_NS = $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) -l $(MOZ_MODULE_LIST_NS)

ifeq (,$(strip $(MOZ_MODULE_LIST)))
FASTUPDATE_MODULES = $(error No modules or projects were specified. Use MOZ_CO_PROJECT to specify a project for checkout.)
CHECKOUT_MODULES   = $(error No modules or projects were specified. Use MOZ_CO_PROJECT to specify a project for checkout.)
else
FASTUPDATE_MODULES := fast_update $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(MOZ_MODULE_LIST)
CHECKOUT_MODULES   := cvs_co $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(MOZ_MODULE_LIST);
endif
ifeq (,$(MOZ_MODULE_LIST_NS))
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

CVSCO_LOCALES := $(CVS) $(CVS_FLAGS) -d $(LOCALES_CVSROOT) co $(LOCALES_CO_FLAGS) $(CVS_CO_LOCALES_DATE_FLAGS) $(LOCALE_CO_DIRS)

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
	$(CHECKOUT_MODULES_NS); \
	$(CHECKOUT_MODULES) \
	$(CHECKOUT_LOCALES);
	@echo "checkout finish: "`date` | tee -a $(CVSCO_LOGFILE)
# update the NSS checkout timestamp, if we checked PSM out
	@if test -d $(TOPSRCDIR)/security/manager -a \
		 `egrep -c '^(U|C) mozilla/security/(nss|coreconf)' $(CVSCO_LOGFILE) 2>/dev/null` != 0; then \
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

CVSCO_LOGFILE_L10N := $(ROOTDIR)/cvsco-l10n.log
CVSCO_LOGFILE_L10N := $(shell echo $(CVSCO_LOGFILE_L10N) | sed s%//%/%)

l10n-checkout:
#	@: Backup the last checkout log.
	@if test -f $(CVSCO_LOGFILE_L10N) ; then \
	  mv $(CVSCO_LOGFILE_L10N) $(CVSCO_LOGFILE_L10N).old; \
	else true; \
	fi
	@echo "checkout start: "`date` | tee $(CVSCO_LOGFILE_L10N)
	@echo '$(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/client.mk $(MOZCONFIG_MODULES)'; \
        cd $(ROOTDIR) && \
	$(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/client.mk $(MOZCONFIG_MODULES)
	@cd $(ROOTDIR) && $(MAKE) -f mozilla/client.mk real_l10n-checkout

EN_US_CO_DIRS := $(sort $(foreach dir,$(LOCALE_DIRS),mozilla/$(dir)/locales)) \
  $(foreach mod,$(MOZ_PROJECT_LIST),mozilla/$(mod)/config) \
  mozilla/client.mk        \
  $(MOZCONFIG_MODULES)     \
  mozilla/configure        \
  mozilla/configure.in     \
  mozilla/allmakefiles.sh  \
  mozilla/build            \
  mozilla/config           \
  $(NULL)

EN_US_CO_FILES_NS :=          \
  mozilla/toolkit/mozapps/installer \
  $(NULL)

#	Start the checkout. Split the output to the tty and a log file.
real_l10n-checkout:
	@set -e; \
	cvs_co() { set -e; echo "$$@" ; \
	  "$$@" 2>&1 | tee -a $(CVSCO_LOGFILE_L10N); }; \
	cvs_co $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) $(EN_US_CO_DIRS); \
	cvs_co $(CVS) $(CVS_FLAGS) co $(MODULES_CO_FLAGS) $(CVS_CO_DATE_FLAGS) -l $(EN_US_CO_FILES_NS); \
	cvs_co $(CVSCO_LOCALES)
	@echo "checkout finish: "`date` | tee -a $(CVSCO_LOGFILE_L10N)
#	@: Check the log for conflicts. ;
	@conflicts=`egrep "^C " $(CVSCO_LOGFILE_L10N)` ;\
	if test "$$conflicts" ; then \
	  echo "$(MAKE): *** Conflicts during checkout." ;\
	  echo "$$conflicts" ;\
	  echo "$(MAKE): Refer to $(CVSCO_LOGFILE_L10N) for full log." ;\
	  false; \
	else true; \
	fi

#####################################################
# First Checkout

ifdef _IS_FIRST_CHECKOUT
# First time, do build target in a new process to pick up new files.
build::
	$(MAKE) -f $(TOPSRCDIR)/client.mk build
else

#####################################################
# After First Checkout

#####################################################
# Build date unification

ifdef MOZ_UNIFY_BDATE
ifndef MOZ_BUILD_DATE
ifdef MOZ_BUILD_PROJECTS
MOZ_BUILD_DATE = $(shell $(PERL) -I$(TOPSRCDIR)/config $(TOPSRCDIR)/config/bdate.pl)
export MOZ_BUILD_DATE
endif
endif
endif

#####################################################
# Preflight, before building any project

build profiledbuild alldep preflight_all::
ifeq (,$(MOZ_CURRENT_PROJECT)$(if $(MOZ_PREFLIGHT_ALL),,1))
# Don't run preflight_all for individual projects in multi-project builds
# (when MOZ_CURRENT_PROJECT is set.)
ifndef MOZ_BUILD_PROJECTS
# Building a single project, OBJDIR is usable.
	set -e; \
	for mkfile in $(MOZ_PREFLIGHT_ALL); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile preflight_all TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
else
# OBJDIR refers to the project-specific OBJDIR, which is not available at
# this point when building multiple projects.  Only MOZ_OBJDIR is available.
	set -e; \
	for mkfile in $(MOZ_PREFLIGHT_ALL); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile preflight_all TOPSRCDIR=$(TOPSRCDIR) MOZ_OBJDIR=$(MOZ_OBJDIR) MOZ_BUILD_PROJECTS="$(MOZ_BUILD_PROJECTS)"; \
	done
endif
endif

# If we're building multiple projects, but haven't specified which project,
# loop through them.

ifeq (,$(MOZ_CURRENT_PROJECT)$(if $(MOZ_BUILD_PROJECTS),,1))
configure depend build profiledbuild install export libs clean realclean distclean alldep preflight postflight::
	set -e; \
	for app in $(MOZ_BUILD_PROJECTS); do \
	  $(MAKE) -f $(TOPSRCDIR)/client.mk $@ MOZ_CURRENT_PROJECT=$$app; \
	done

else

# MOZ_CURRENT_PROJECT: either doing a single-project build, or building an
# individual project in a multi-project build.

####################################
# Configure

CONFIG_STATUS = $(wildcard $(OBJDIR)/config.status)
CONFIG_CACHE  = $(wildcard $(OBJDIR)/config.cache)

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
  CONFIGURE = ./configure
else
  CONFIGURE = $(TOPSRCDIR)/configure
endif

ifdef MOZ_TOOLS
  CONFIGURE = $(TOPSRCDIR)/configure
endif

configure::
ifdef MOZ_BUILD_PROJECTS
	@if test ! -d $(MOZ_OBJDIR); then $(MKDIR) $(MOZ_OBJDIR); else true; fi
endif
	@if test ! -d $(OBJDIR); then $(MKDIR) $(OBJDIR); else true; fi
	@echo cd $(OBJDIR);
	@echo $(CONFIGURE) $(CONFIGURE_ARGS)
	@cd $(OBJDIR) && $(BUILD_PROJECT_ARG) $(CONFIGURE_ENV_ARGS) $(CONFIGURE) $(CONFIGURE_ARGS) \
	  || ( echo "*** Fix above errors and then restart with\
               \"$(MAKE) -f client.mk build\"" && exit 1 )
	@touch $(OBJDIR)/Makefile

$(OBJDIR)/Makefile $(OBJDIR)/config.status: $(CONFIG_STATUS_DEPS)
	@$(MAKE) -f $(TOPSRCDIR)/client.mk configure

ifneq (,$(CONFIG_STATUS))
$(OBJDIR)/config/autoconf.mk: $(TOPSRCDIR)/config/autoconf.mk.in
	cd $(OBJDIR); \
	  CONFIG_FILES=config/autoconf.mk ./config.status
endif


####################################
# Depend

depend:: $(OBJDIR)/Makefile $(OBJDIR)/config.status
	$(MOZ_MAKE) export && $(MOZ_MAKE) depend

####################################
# Preflight

build profiledbuild alldep preflight::
ifdef MOZ_PREFLIGHT
	set -e; \
	for mkfile in $(MOZ_PREFLIGHT); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile preflight TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
endif

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

####################################
# Postflight

build profiledbuild alldep postflight::
ifdef MOZ_POSTFLIGHT
	set -e; \
	for mkfile in $(MOZ_POSTFLIGHT); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile postflight TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
endif

endif # MOZ_CURRENT_PROJECT

####################################
# Postflight, after building all projects

build profiledbuild alldep postflight_all::
ifeq (,$(MOZ_CURRENT_PROJECT)$(if $(MOZ_POSTFLIGHT_ALL),,1))
# Don't run postflight_all for individual projects in multi-project builds
# (when MOZ_CURRENT_PROJECT is set.)
ifndef MOZ_BUILD_PROJECTS
# Building a single project, OBJDIR is usable.
	set -e; \
	for mkfile in $(MOZ_POSTFLIGHT_ALL); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile postflight_all TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
else
# OBJDIR refers to the project-specific OBJDIR, which is not available at
# this point when building multiple projects.  Only MOZ_OBJDIR is available.
	set -e; \
	for mkfile in $(MOZ_POSTFLIGHT_ALL); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile postflight_all TOPSRCDIR=$(TOPSRCDIR) MOZ_OBJDIR=$(MOZ_OBJDIR) MOZ_BUILD_PROJECTS="$(MOZ_BUILD_PROJECTS)"; \
	done
endif
endif

cleansrcdir:
	@cd $(TOPSRCDIR); \
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

.PHONY: checkout real_checkout depend build export libs alldep install clean realclean distclean cleansrcdir pull_all build_all clobber clobber_all pull_and_build_all everything configure preflight_all preflight postflight postflight_all
