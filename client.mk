# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
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
#   Joey Armstrong <joey@mozilla.com>
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
# To build a tree,
#    1. hg clone ssh://hg.mozilla.org/mozilla-central mozilla
#    2. cd mozilla
#    3. create your .mozconfig file with
#       ac_add_options --enable-application=browser
#    4. gmake -f client.mk
#
# Other targets (gmake -f client.mk [targets...]),
#    build
#    clean (realclean is now the same as clean)
#    distclean
#
# See http://developer.mozilla.org/en/docs/Build_Documentation for 
# more information.
#
# Options:
#   MOZ_BUILD_PROJECTS   - Build multiple projects in subdirectories
#                          of MOZ_OBJDIR
#   MOZ_OBJDIR           - Destination object directory
#   MOZ_MAKE_FLAGS       - Flags to pass to $(MAKE)
#   MOZ_PREFLIGHT_ALL  } - Makefiles to run before any project in
#   MOZ_PREFLIGHT      }   MOZ_BUILD_PROJECTS, before each project, after
#   MOZ_POSTFLIGHT     }   each project, and after all projects; these
#   MOZ_POSTFLIGHT_ALL }   variables contain space-separated lists
#   MOZ_UNIFY_BDATE      - Set to use the same bdate for each project in
#                          MOZ_BUILD_PROJECTS
#
#######################################################################
# Defines

comma := ,

CWD := $(CURDIR)
ifneq (1,$(words $(CWD)))
$(error The mozilla directory cannot be located in a path with spaces.)
endif

ifeq "$(CWD)" "/"
CWD   := /.
endif

ifndef TOPSRCDIR
ifeq (,$(wildcard client.mk))
TOPSRCDIR := $(patsubst %/,%,$(dir $(MAKEFILE_LIST)))
MOZ_OBJDIR = .
else
TOPSRCDIR := $(CWD)
endif
endif

# try to find autoconf 2.13 - discard errors from 'which'
# MacOS X 10.4 sends "no autoconf*" errors to stdout, discard those via grep
AUTOCONF ?= $(shell which autoconf-2.13 autoconf2.13 autoconf213 2>/dev/null | grep -v '^no autoconf' | head -1)

ifeq (,$(strip $(AUTOCONF)))
AUTOCONF=$(error Could not find autoconf 2.13)
endif

MKDIR := mkdir
SH := /bin/sh
PERL ?= perl
PYTHON ?= python

CONFIG_GUESS_SCRIPT := $(wildcard $(TOPSRCDIR)/build/autoconf/config.guess)
ifdef CONFIG_GUESS_SCRIPT
  CONFIG_GUESS = $(shell $(CONFIG_GUESS_SCRIPT))
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
# Load mozconfig Options

# See build pages, http://www.mozilla.org/build/ for how to set up mozconfig.

MOZCONFIG_LOADER := build/autoconf/mozconfig2client-mk
MOZCONFIG_FINDER := build/autoconf/mozconfig-find 
MOZCONFIG_MODULES := build/unix/uniq.pl



run_for_side_effects := \
  $(shell $(TOPSRCDIR)/$(MOZCONFIG_LOADER) $(TOPSRCDIR) $(TOPSRCDIR)/.mozconfig.mk > $(TOPSRCDIR)/.mozconfig.out)

include $(TOPSRCDIR)/.mozconfig.mk

ifndef MOZ_OBJDIR
  MOZ_OBJDIR = obj-$(CONFIG_GUESS)
endif

ifdef MOZ_BUILD_PROJECTS

ifdef MOZ_CURRENT_PROJECT
  OBJDIR = $(MOZ_OBJDIR)/$(MOZ_CURRENT_PROJECT)
  MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)
  BUILD_PROJECT_ARG = MOZ_BUILD_APP=$(MOZ_CURRENT_PROJECT)
else
  OBJDIR = $(error Cannot find the OBJDIR when MOZ_CURRENT_PROJECT is not set.)
  MOZ_MAKE = $(error Cannot build in the OBJDIR when MOZ_CURRENT_PROJECT is not set.)
endif

else # MOZ_BUILD_PROJECTS

OBJDIR = $(MOZ_OBJDIR)
MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)

endif # MOZ_BUILD_PROJECTS

# 'configure' scripts generated by autoconf.
CONFIGURES := $(TOPSRCDIR)/configure
CONFIGURES += $(TOPSRCDIR)/js/src/configure

#######################################################################
# Rules

# The default rule is build
build::
	$(MAKE) -f $(TOPSRCDIR)/client.mk $(if $(MOZ_PGO),profiledbuild,realbuild)


# Print out any options loaded from mozconfig.
all realbuild clean depend distclean export libs install realclean::
	@if test -f .mozconfig.out; then \
	  cat .mozconfig.out; \
	  rm -f .mozconfig.out; \
	else true; \
	fi

# Windows equivalents
build_all: build
build_all_dep: alldep
build_all_depend: alldep
clobber clobber_all: clean

# Do everything from scratch
everything: clean build

####################################
# Profile-Guided Optimization
#  To use this, you should set the following variables in your mozconfig
#    mk_add_options PROFILE_GEN_SCRIPT=/path/to/profile-script
#
#  The profile script should exercise the functionality to be included
#  in the profile feedback.
#
#  This is up here, outside of the MOZ_CURRENT_PROJECT logic so that this
#  is usable in multi-pass builds, where you might not have a runnable
#  application until all the build passes and postflight scripts have run.
ifdef MOZ_OBJDIR
  PGO_OBJDIR = $(MOZ_OBJDIR)
else
  PGO_OBJDIR := $(TOPSRCDIR)
endif

profiledbuild::
	$(MAKE) -f $(TOPSRCDIR)/client.mk realbuild MOZ_PROFILE_GENERATE=1
	$(MAKE) -C $(PGO_OBJDIR) package
	OBJDIR=${PGO_OBJDIR} JARLOG_DIR=${PGO_OBJDIR}/jarlog/en-US $(PROFILE_GEN_SCRIPT)
	$(MAKE) -f $(TOPSRCDIR)/client.mk maybe_clobber_profiledbuild
	$(MAKE) -f $(TOPSRCDIR)/client.mk realbuild MOZ_PROFILE_USE=1

#####################################################
# Build date unification

ifdef MOZ_UNIFY_BDATE
ifndef MOZ_BUILD_DATE
ifdef MOZ_BUILD_PROJECTS
MOZ_BUILD_DATE = $(shell $(PYTHON) $(TOPSRCDIR)/toolkit/xre/make-platformini.py --print-buildid)
export MOZ_BUILD_DATE
endif
endif
endif

#####################################################
# Preflight, before building any project

realbuild alldep preflight_all::
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
configure depend realbuild install export libs clean realclean distclean alldep preflight postflight maybe_clobber_profiledbuild upload sdk::
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

EXTRA_CONFIG_DEPS := \
	$(TOPSRCDIR)/aclocal.m4 \
	$(wildcard $(TOPSRCDIR)/build/autoconf/*.m4) \
	$(TOPSRCDIR)/js/src/aclocal.m4 \
	$(NULL)

$(CONFIGURES): %: %.in $(EXTRA_CONFIG_DEPS)
	@$(PYTHON) $(TOPSRCDIR)/js/src/config/check-sync-dirs.py $(TOPSRCDIR)/js/src/build $(TOPSRCDIR)/build
	@echo Generating $@ using autoconf
	cd $(@D); $(AUTOCONF)

CONFIG_STATUS_DEPS := \
	$(wildcard $(CONFIGURES)) \
	$(TOPSRCDIR)/allmakefiles.sh \
	$(TOPSRCDIR)/.mozconfig.mk \
	$(wildcard $(TOPSRCDIR)/nsprpub/configure) \
	$(wildcard $(TOPSRCDIR)/config/milestone.txt) \
	$(wildcard $(TOPSRCDIR)/js/src/config/milestone.txt) \
	$(wildcard $(TOPSRCDIR)/browser/config/version.txt) \
	$(wildcard $(addsuffix confvars.sh,$(wildcard $(TOPSRCDIR)/*/))) \
	$(NULL)

# configure uses the program name to determine @srcdir@. Calling it without
#   $(TOPSRCDIR) will set @srcdir@ to "."; otherwise, it is set to the full
#   path of $(TOPSRCDIR).
ifeq ($(TOPSRCDIR),$(OBJDIR))
  CONFIGURE = ./configure
else
  CONFIGURE = $(TOPSRCDIR)/configure
endif

configure-files: $(CONFIGURES)

configure:: configure-files
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

realbuild alldep preflight::
ifdef MOZ_PREFLIGHT
	set -e; \
	for mkfile in $(MOZ_PREFLIGHT); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile preflight TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
endif

####################################
# Build it

realbuild::  $(OBJDIR)/Makefile $(OBJDIR)/config.status
	@$(PYTHON) $(TOPSRCDIR)/js/src/config/check-sync-dirs.py $(TOPSRCDIR)/js/src/config $(TOPSRCDIR)/config
	$(MOZ_MAKE)

####################################
# Other targets

# Pass these target onto the real build system
install export libs clean realclean distclean alldep maybe_clobber_profiledbuild upload sdk:: $(OBJDIR)/Makefile $(OBJDIR)/config.status
	$(MOZ_MAKE) $@

####################################
# Postflight

realbuild alldep postflight::
ifdef MOZ_POSTFLIGHT
	set -e; \
	for mkfile in $(MOZ_POSTFLIGHT); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile postflight TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
endif

endif # MOZ_CURRENT_PROJECT

####################################
# Postflight, after building all projects

realbuild alldep postflight_all::
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

echo-variable-%:
	@echo $($*)

# This makefile doesn't support parallel execution. It does pass
# MOZ_MAKE_FLAGS to sub-make processes, so they will correctly execute
# in parallel.
.NOTPARALLEL:

.PHONY: checkout real_checkout depend realbuild build profiledbuild maybe_clobber_profiledbuild export libs alldep install clean realclean distclean cleansrcdir pull_all build_all clobber clobber_all pull_and_build_all everything configure preflight_all preflight postflight postflight_all upload sdk
