# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
else
TOPSRCDIR := $(CWD)
endif
endif

# try to find autoconf 2.13 - discard errors from 'which'
# MacOS X 10.4 sends "no autoconf*" errors to stdout, discard those via grep
AUTOCONF ?= $(shell which autoconf-2.13 autoconf2.13 autoconf213 2>/dev/null | grep -v '^no autoconf' | head -1)

# See if the autoconf package was installed through fink
ifeq (,$(strip $(AUTOCONF)))
AUTOCONF = $(shell which fink >/dev/null 2>&1 && echo `which fink`/../../lib/autoconf2.13/bin/autoconf)
endif

ifeq (,$(strip $(AUTOCONF)))
AUTOCONF=$(error Could not find autoconf 2.13)
endif

SH := /bin/sh
PERL ?= perl
PYTHON ?= $(shell which python2.7 > /dev/null 2>&1 && echo python2.7 || echo python)

CONFIG_GUESS_SCRIPT := $(wildcard $(TOPSRCDIR)/build/autoconf/config.guess)
ifdef CONFIG_GUESS_SCRIPT
  CONFIG_GUESS := $(shell $(CONFIG_GUESS_SCRIPT))
endif


####################################
# Sanity checks

# Windows checks.
ifneq (,$(findstring mingw,$(CONFIG_GUESS)))

# check for CRLF line endings
ifneq (0,$(shell $(PERL) -e 'binmode(STDIN); while (<STDIN>) { if (/\r/) { print "1"; exit } } print "0"' < $(TOPSRCDIR)/client.mk))
$(error This source tree appears to have Windows-style line endings. To \
convert it to Unix-style line endings, check \
"https://developer.mozilla.org/en-US/docs/Developer_Guide/Mozilla_build_FAQ\#Win32-specific_questions" \
for a workaround of this issue.)
endif

# Set this for baseconfig.mk
HOST_OS_ARCH=WINNT
endif

# Include baseconfig.mk for its $(MAKE) validation.
include $(TOPSRCDIR)/config/baseconfig.mk

####################################
# Load mozconfig Options

# See build pages, http://www.mozilla.org/build/ for how to set up mozconfig.

define CR


endef

# As $(shell) doesn't preserve newlines, use sed to replace them with an
# unlikely sequence (||), which is then replaced back to newlines by make
# before evaluation. $(shell) replacing newlines with spaces, || is always
# followed by a space (since sed doesn't remove newlines), except on the
# last line, so replace both '|| ' and '||'.
# Also, make MOZ_PGO available to mozconfig when passed on make command line.
# Likewise for MOZ_CURRENT_PROJECT.
MOZCONFIG_CONTENT := $(subst ||,$(CR),$(subst || ,$(CR),$(shell $(addprefix MOZ_CURRENT_PROJECT=,$(MOZ_CURRENT_PROJECT)) MOZ_PGO=$(MOZ_PGO) $(TOPSRCDIR)/mach environment --format=client.mk | sed 's/$$/||/')))
$(eval $(MOZCONFIG_CONTENT))

export FOUND_MOZCONFIG

# As '||' was used as a newline separator, it means it's not occurring in
# lines themselves. It can thus safely be used to replaces normal spaces,
# to then replace newlines with normal spaces. This allows to get a list
# of mozconfig output lines.
MOZCONFIG_OUT_LINES := $(subst $(CR), ,$(subst $(NULL) $(NULL),||,$(MOZCONFIG_CONTENT)))
# Filter-out comments from those lines.
START_COMMENT = \#
MOZCONFIG_OUT_FILTERED := $(filter-out $(START_COMMENT)%,$(MOZCONFIG_OUT_LINES))

ifdef AUTOCLOBBER
export AUTOCLOBBER=1
endif
export MOZ_PGO

ifdef MOZ_PARALLEL_BUILD
  MOZ_MAKE_FLAGS := $(filter-out -j%,$(MOZ_MAKE_FLAGS))
  MOZ_MAKE_FLAGS += -j$(MOZ_PARALLEL_BUILD)
endif

# Automatically add -jN to make flags if not defined. N defaults to number of cores.
ifeq (,$(findstring -j,$(MOZ_MAKE_FLAGS)))
  cores=$(shell $(PYTHON) -c 'import multiprocessing; print(multiprocessing.cpu_count())')
  MOZ_MAKE_FLAGS += -j$(cores)
endif


ifdef MOZ_BUILD_PROJECTS

ifdef MOZ_CURRENT_PROJECT
  BUILD_PROJECT_ARG = MOZ_BUILD_APP=$(MOZ_CURRENT_PROJECT)
  export MOZ_CURRENT_PROJECT
else
  MOZ_MAKE = $(error Cannot build in the OBJDIR when MOZ_CURRENT_PROJECT is not set.)
endif
endif # MOZ_BUILD_PROJECTS

MOZ_MAKE = $(MAKE) $(MOZ_MAKE_FLAGS) -C $(OBJDIR)

# 'configure' scripts generated by autoconf.
CONFIGURES := $(TOPSRCDIR)/configure
CONFIGURES += $(TOPSRCDIR)/js/src/configure

# Make targets that are going to be passed to the real build system
OBJDIR_TARGETS = install export libs clean realclean distclean maybe_clobber_profiledbuild upload sdk installer package package-compare stage-package source-package l10n-check automation/build

#######################################################################
# Rules

# The default rule is build
build::
	$(MAKE) -f $(TOPSRCDIR)/client.mk $(if $(MOZ_PGO),profiledbuild,realbuild) CREATE_MOZCONFIG_JSON=

# Define mkdir
include $(TOPSRCDIR)/config/makefiles/makeutils.mk
include $(TOPSRCDIR)/config/makefiles/autotargets.mk

# Create a makefile containing the mk_add_options values from mozconfig,
# but only do so when OBJDIR is defined (see further above).
ifdef MOZ_BUILD_PROJECTS
ifdef MOZ_CURRENT_PROJECT
WANT_MOZCONFIG_MK = 1
else
WANT_MOZCONFIG_MK =
endif
else
WANT_MOZCONFIG_MK = 1
endif

ifdef WANT_MOZCONFIG_MK
# For now, only output "export" lines from mach environment --format=client.mk output.
MOZCONFIG_MK_LINES := $(filter export||%,$(MOZCONFIG_OUT_LINES))
$(OBJDIR)/.mozconfig.mk: $(FOUND_MOZCONFIG) $(call mkdir_deps,$(OBJDIR)) $(OBJDIR)/CLOBBER
	$(if $(MOZCONFIG_MK_LINES),( $(foreach line,$(MOZCONFIG_MK_LINES), echo '$(subst ||, ,$(line))';) )) > $@

# Include that makefile so that it is created. This should not actually change
# the environment since MOZCONFIG_CONTENT, which MOZCONFIG_OUT_LINES derives
# from, has already been eval'ed.
include $(OBJDIR)/.mozconfig.mk
endif

# UPLOAD_EXTRA_FILES is appended to and exported from mozconfig, which makes
# submakes as well as configure add even more to that, so just unexport it
# for submakes to pick it from .mozconfig.mk and for configure to pick it
# from mach environment.
unexport UPLOAD_EXTRA_FILES

# Print out any options loaded from mozconfig.
all realbuild clean distclean export libs install realclean::
ifneq (,$(strip $(MOZCONFIG_OUT_FILTERED)))
	$(info Adding client.mk options from $(FOUND_MOZCONFIG):)
	$(foreach line,$(MOZCONFIG_OUT_FILTERED),$(info $(NULL) $(NULL) $(NULL) $(NULL) $(subst ||, ,$(line))))
endif

# Windows equivalents
build_all: build
clobber clobber_all: clean

# helper target for mobile
build_and_deploy: build package install

# Do everything from scratch
everything: clean build

####################################
# Profile-Guided Optimization
#  This is up here, outside of the MOZ_CURRENT_PROJECT logic so that this
#  is usable in multi-pass builds, where you might not have a runnable
#  application until all the build passes and postflight scripts have run.
profiledbuild::
	$(MAKE) -f $(TOPSRCDIR)/client.mk realbuild MOZ_PROFILE_GENERATE=1 MOZ_PGO_INSTRUMENTED=1 CREATE_MOZCONFIG_JSON=
	$(MAKE) -C $(OBJDIR) package MOZ_PGO_INSTRUMENTED=1 MOZ_INTERNAL_SIGNING_FORMAT= MOZ_EXTERNAL_SIGNING_FORMAT=
	rm -f $(OBJDIR)/jarlog/en-US.log
	MOZ_PGO_INSTRUMENTED=1 JARLOG_FILE=jarlog/en-US.log EXTRA_TEST_ARGS=10 $(MAKE) -C $(OBJDIR) pgo-profile-run
	$(MAKE) -f $(TOPSRCDIR)/client.mk maybe_clobber_profiledbuild CREATE_MOZCONFIG_JSON=
	$(MAKE) -f $(TOPSRCDIR)/client.mk realbuild MOZ_PROFILE_USE=1 CREATE_MOZCONFIG_JSON=

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

realbuild preflight_all::
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
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile preflight_all TOPSRCDIR=$(TOPSRCDIR) MOZ_OBJDIR=$(MOZ_OBJDIR) MOZ_BUILD_PROJECTS='$(MOZ_BUILD_PROJECTS)'; \
	done
endif
endif

# If we're building multiple projects, but haven't specified which project,
# loop through them.

ifeq (,$(MOZ_CURRENT_PROJECT)$(if $(MOZ_BUILD_PROJECTS),,1))
configure realbuild preflight postflight $(OBJDIR_TARGETS)::
	set -e; \
	for app in $(MOZ_BUILD_PROJECTS); do \
	  $(MAKE) -f $(TOPSRCDIR)/client.mk $@ MOZ_CURRENT_PROJECT=$$app; \
	done

else

# MOZ_CURRENT_PROJECT: either doing a single-project build, or building an
# individual project in a multi-project build.

####################################
# Configure

MAKEFILE      = $(wildcard $(OBJDIR)/Makefile)
CONFIG_STATUS = $(wildcard $(OBJDIR)/config.status)
CONFIG_CACHE  = $(wildcard $(OBJDIR)/config.cache)

EXTRA_CONFIG_DEPS := \
  $(TOPSRCDIR)/aclocal.m4 \
  $(wildcard $(TOPSRCDIR)/build/autoconf/*.m4) \
  $(TOPSRCDIR)/js/src/aclocal.m4 \
  $(NULL)

$(CONFIGURES): %: %.in $(EXTRA_CONFIG_DEPS)
	@echo Generating $@ using autoconf
	cd $(@D); $(AUTOCONF)

CONFIG_STATUS_DEPS := \
  $(wildcard $(TOPSRCDIR)/*/confvars.sh) \
  $(CONFIGURES) \
  $(TOPSRCDIR)/CLOBBER \
  $(TOPSRCDIR)/nsprpub/configure \
  $(TOPSRCDIR)/config/milestone.txt \
  $(TOPSRCDIR)/browser/config/version.txt \
  $(TOPSRCDIR)/build/virtualenv_packages.txt \
  $(TOPSRCDIR)/python/mozbuild/mozbuild/virtualenv.py \
  $(TOPSRCDIR)/testing/mozbase/packages.txt \
  $(OBJDIR)/.mozconfig.json \
  $(NULL)

CONFIGURE_ENV_ARGS += \
  MAKE='$(MAKE)' \
  $(NULL)

# configure uses the program name to determine @srcdir@. Calling it without
#   $(TOPSRCDIR) will set @srcdir@ to "."; otherwise, it is set to the full
#   path of $(TOPSRCDIR).
ifeq ($(TOPSRCDIR),$(OBJDIR))
  CONFIGURE = ./configure
else
  CONFIGURE = $(TOPSRCDIR)/configure
endif

$(OBJDIR)/CLOBBER: $(TOPSRCDIR)/CLOBBER
	$(PYTHON) $(TOPSRCDIR)/config/pythonpath.py -I $(TOPSRCDIR)/testing/mozbase/mozfile \
	    $(TOPSRCDIR)/python/mozbuild/mozbuild/controller/clobber.py $(TOPSRCDIR) $(OBJDIR)

configure-files: $(CONFIGURES)

configure-preqs = \
  $(OBJDIR)/CLOBBER \
  configure-files \
  $(call mkdir_deps,$(OBJDIR)) \
  $(if $(MOZ_BUILD_PROJECTS),$(call mkdir_deps,$(MOZ_OBJDIR))) \
  save-mozconfig \
  $(OBJDIR)/.mozconfig.json \
  $(NULL)

CREATE_MOZCONFIG_JSON = $(shell $(TOPSRCDIR)/mach environment --format=json -o $(OBJDIR)/.mozconfig.json)
# Force CREATE_MOZCONFIG_JSON above to be resolved, without side effects in
# case the result is non empty, and allowing an override on the make command
# line not running the command (using := $(shell) still runs the shell command).
ifneq (,$(CREATE_MOZCONFIG_JSON))
endif

$(OBJDIR)/.mozconfig.json: $(call mkdir_deps,$(OBJDIR)) ;

save-mozconfig: $(FOUND_MOZCONFIG)
	-cp $(FOUND_MOZCONFIG) $(OBJDIR)/.mozconfig

configure:: $(configure-preqs)
	@echo cd $(OBJDIR);
	@echo $(CONFIGURE) $(CONFIGURE_ARGS)
	@cd $(OBJDIR) && $(BUILD_PROJECT_ARG) $(CONFIGURE_ENV_ARGS) $(CONFIGURE) $(CONFIGURE_ARGS) \
	  || ( echo '*** Fix above errors and then restart with\
               "$(MAKE) -f client.mk build"' && exit 1 )
	@touch $(OBJDIR)/Makefile

ifneq (,$(MAKEFILE))
$(OBJDIR)/Makefile: $(OBJDIR)/config.status

$(OBJDIR)/config.status: $(CONFIG_STATUS_DEPS)
else
$(OBJDIR)/Makefile: $(CONFIG_STATUS_DEPS)
endif
	@$(MAKE) -f $(TOPSRCDIR)/client.mk configure CREATE_MOZCONFIG_JSON=

ifneq (,$(CONFIG_STATUS))
$(OBJDIR)/config/autoconf.mk: $(TOPSRCDIR)/config/autoconf.mk.in
	$(PYTHON) $(OBJDIR)/config.status -n --file=$(OBJDIR)/config/autoconf.mk
endif


####################################
# Preflight

realbuild preflight::
ifdef MOZ_PREFLIGHT
	set -e; \
	for mkfile in $(MOZ_PREFLIGHT); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile preflight TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
endif

####################################
# Build it

realbuild::  $(OBJDIR)/Makefile $(OBJDIR)/config.status
	+$(MOZ_MAKE)

####################################
# Other targets

# Pass these target onto the real build system
$(OBJDIR_TARGETS):: $(OBJDIR)/Makefile $(OBJDIR)/config.status
	+$(MOZ_MAKE) $@

####################################
# Postflight

realbuild postflight::
ifdef MOZ_POSTFLIGHT
	set -e; \
	for mkfile in $(MOZ_POSTFLIGHT); do \
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile postflight TOPSRCDIR=$(TOPSRCDIR) OBJDIR=$(OBJDIR) MOZ_OBJDIR=$(MOZ_OBJDIR); \
	done
endif

endif # MOZ_CURRENT_PROJECT

####################################
# Postflight, after building all projects

realbuild postflight_all::
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
	  $(MAKE) -f $(TOPSRCDIR)/$$mkfile postflight_all TOPSRCDIR=$(TOPSRCDIR) MOZ_OBJDIR=$(MOZ_OBJDIR) MOZ_BUILD_PROJECTS='$(MOZ_BUILD_PROJECTS)'; \
	done
endif
endif

cleansrcdir:
	@cd $(TOPSRCDIR); \
	if [ -f Makefile ]; then \
	  $(MAKE) distclean ; \
	else \
	  echo 'Removing object files from srcdir...'; \
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

.PHONY: checkout \
    real_checkout \
    realbuild \
    build \
    profiledbuild \
    cleansrcdir \
    pull_all \
    build_all \
    clobber \
    clobber_all \
    pull_and_build_all \
    everything \
    configure \
    preflight_all \
    preflight \
    postflight \
    postflight_all \
    $(OBJDIR_TARGETS)
