#!gmake
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

# Build the Mozilla client.
#
# This needs CVSROOT set to work, e.g.,
#   setenv CVSROOT :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
# or
#   setenv CVSROOT :pserver:username%somedomain.org@cvs.mozilla.org:/cvsroot
# 
# To checkout and build a new tree,
#    1. cvs co mozilla/client.mk
#    2. cd mozilla
#    3. gmake -f client.mk webconfig
#       (or goto http://cvs-mirror.mozilla.org/webtools/build/config.cgi)
#    4. gmake -f client.mk
#
# To checkout (update) and build an existing tree,
#    gmake -f client.mk
#
# Other targets (gmake -f client.mk [targets...]),
#    checkout  (also pull_all)
#    build     (also build_all)
#    realclean (also clobber_all)
#    clean     (also clobber)
#
# The nspr library is handled as follows,
#    Read $HOME/.mozconfig.sh (or $(TOPSRCDIR)/mozconfig.sh) and
#    get the directory specified by --with-nspr.
#    If the flag is not there, look for nspr in /usr/bin.
#    Otherwise, build from tip and install in $(OBJDIR)/dist/nspr
#
# See http://www.mozilla.org/build/unix.html for more information.
#
# Options:
#   MOZ_WITH_NSPR       - Nspr directory for configure --with-nspr=
#   MOZ_WITH_PTHREADS   - Pthreads for nspr and configure
#   MOZ_OBJDIR          - Destination object directory
# also,
#   MOZ_CO_BRANCH       - Branch tag to use for checkout (default: HEAD)
#   MOZ_CO_MODULE       - Module to checkout (default: SeaMonkeyEditor)
#   MOZ_CVS_FLAGS       - Flags to pass cvs (default: -q -z3)
#   MOZ_CO_FLAGS        - Flags to pass after 'cvs co' (default: -P)

CWD		:= $(shell pwd)
ifeq (mozilla, $(notdir $(CWD)))
ROOTDIR		:= $(shell dirname $(CWD))
TOPSRCDIR       := $(CWD)
else
ROOTDIR		:= $(CWD)
TOPSRCDIR       := $(CWD)/mozilla
endif

AUTOCONF	:= autoconf
MKDIR		:= mkdir
SH		:= /bin/sh
ifndef MAKE
MAKE		:= gmake
endif

WEBCONFIG_URL   := http://cvs-mirror.mozilla.org/webtools/build/config.cgi
WEBCONFIG_FILE  := $(HOME)/.mozconfig.sh

CONFIG_GUESS	:= $(wildcard $(TOPSRCDIR)/build/autoconf/config.guess)
ifdef CONFIG_GUESS
  CONFIG_GUESS := $(shell $(CONFIG_GUESS))
else
  IS_FIRST_CHECKOUT := 1
endif

# Load options from mozconfig.sh
#   (See build pages, http://www.mozilla.org/build/unix.html, 
#    for how to set up mozconfig.sh.)
MOZCONFIG2DEFS := build/autoconf/mozconfig2defs.sh
FIND_MOZCONFIG := build/autoconf/find-mozconfig.sh
run_for_side_effects := \
  $(shell cd $(TOPSRCDIR); \
          if test ! -f $(MOZCONFIG2DEFS); then \
	    (cd ..; cvs co mozilla/$(MOZCONFIG2DEFS) mozilla/$(FIND_MOZCONFIG); ) \
	  else true; \
	  fi; \
	  $(MOZCONFIG2DEFS) .client-defs.mk)
include $(TOPSRCDIR)/.client-defs.mk

ifdef MOZ_OBJDIR
  OBJDIR := $(MOZ_OBJDIR)
else
# Default objdir, e.g. mozilla/obj-i686-pc-linux-gnu
  OBJDIR :=
  ifdef CONFIG_GUESS
    OBJDIR := $(ROOTDIR)/obj-$(CONFIG_GUESS)
  endif
endif

# 
# Step 1: CVS
#

# Basic CVS flags
ifdef MOZ_CVS_FLAGS
  CVS_CFLAGS := $(MOZ_CVS_FLAGS)
else
  CVS_CFLAGS := -q -z 3
endif

# Anything that we should use on all checkouts
ifdef MOZ_CO_FLAGS
  CVS_COFLAGS := $(MOZ_CO_FLAGS)
else
  CVS_COFLAGS := -P
endif

# The default branch tag
ifdef MOZ_CO_BRANCH
  CVS_BRANCH_FLAGS := -r $(MOZ_CO_BRANCH)
endif

ifndef MOZ_CO_MODULE
  MOZ_CO_MODULE := SeaMonkeyEditor
endif

CVS		:= cvs $(CVS_CFLAGS)
CVSCO		:= $(CVS) co $(CVS_COFLAGS) $(CVS_BRANCH_FLAGS)
CVSCO_LOGFILE	:= $(ROOTDIR)/cvsco.log

#
# Step 2: NSPR
#

ifeq ($(MOZ_WITH_PTHREADS), 1)
NSPR_PTHREAD_FLAG := USE_PTHREADS=1
endif

NSPR_BRANCH :=
#NSPR_BRANCH := -rNSPRPUB_RELEASE_3_0

# These options can be overriden by the user

ifdef MOZ_WITH_NSPR
NSPR_INSTALL_DIR := $(MOZ_WITH_NSPR)
NEED_WITH_NSPR   := 1
else
ifneq ("$(wildcard /usr/lib/libnspr3*)","")
NSPR_INSTALL_DIR := /usr
else
NSPR_INSTALL_DIR := $(OBJDIR)/nspr
NEED_WITH_NSPR   := 1
endif
endif

ifndef NSPR_OPTIONS
NSPR_OPTIONS := NS_USE_GCC=1 NS_USE_NATIVE= $(NSPR_PTHREAD_FLAG)
endif

# These options are required to make this makefile build NSPR correctly
NSPR_REQ_OPTIONS := MOZILLA_CLIENT=1 NO_MDUPDATE=1
NSPR_DIST_OPTION := DIST=$(NSPR_INSTALL_DIR) NSDISTMODE=copy
NSPR_TARGET := export

NSPR_GMAKE_OPTIONS := \
	$(NSPR_DIST_OPTION) \
	$(NSPR_REQ_OPTIONS) \
	$(NSPR_OPTIONS) \
	$(NSPR_TARGET)

#
# Step 3: autoconf
#

CONFIG_FLAGS :=

ifdef NEED_WITH_NSPR
CONFIG_FLAGS += --with-nspr=$(NSPR_INSTALL_DIR)
endif

ifeq "$(origin MOZ_WITH_PTHREADS)" "environment"
CONFIG_FLAGS += --with-pthreads
endif

#
# Rules
# 

all: checkout build

.PHONY: checkout nspr build clean realclean

# Windows equivalents
pull_all:     checkout
build_all:    build
clobber:      clean
clobber_all:  realclean
pull_and_build_all: checkout build

#
# CVS checkout
#
checkout:
	@if test -f $(CVSCO_LOGFILE) ; then \
	  mv $(CVSCO_LOGFILE) $(CVSCO_LOGFILE).old; \
	else true; \
	fi
	@date > $(CVSCO_LOGFILE)
	cd $(ROOTDIR) && \
	  $(CVSCO) $(MOZ_CO_MODULE) 2>&1 | tee -a $(CVSCO_LOGFILE)
	@conflicts=`egrep "^C " $(CVSCO_LOGFILE)` ;\
	if test "$$conflicts" ; then \
	  echo "$(MAKE): *** Conflicts during checkout." ;\
	  echo "$$conflicts" ;\
	  echo "$(MAKE): Refer to $(CVSCO_LOGFILE) for full log." ;\
	  exit 1; \
	else true; \
	fi

#
# Web configure
#
MOZCONFIG2URL := build/autoconf/mozconfig2url.sh
webconfig:
	@url=$(WEBCONFIG_URL); \
	if test -f $(WEBCONFIG_FILE) ; then \
          cd $(TOPSRCDIR); \
          if test ! -f $(MOZCONFIG2URL); then \
	    (cd ..; cvs co mozilla/$(MOZCONFIG2URL) mozilla/$(FIND_MOZCONFIG);) \
	  else true; \
	  fi; \
	  url=$$url`$(MOZCONFIG2URL)`; \
	else true; \
	fi; \
	echo Running netscape with the following url: ;\
	echo ;\
	echo $$url ;\
	netscape -remote "openURL($$url)" || netscape $$url ;\
	echo ;\
	echo Fill out the form on the browser. ;\
	echo Save the results to $(WEBCONFIG_FILE) when done.

#	netscape -remote "saveAs($(WEBCONFIG_FILE))"

ifdef IS_FIRST_CHECKOUT
# First time, do build target in a new process to pick up new files.
build:
	$(MAKE) -f $(TOPSRCDIR)/client.mk build
else

#
# Configure
#

CONFIG_STATUS := $(wildcard $(OBJDIR)/config.status)
CONFIG_CACHE  := $(wildcard $(OBJDIR)/config.cache)

ifdef RUN_AUTOCONF_LOCALLY
EXTRA_CONFIG_DEPS := \
	$(TOPSRCDIR)/aclocal.m4 \
	$(TOPSRCDIR)/build/autoconf/gtk.m4 \
	$(TOPSRCDIR)/build/autoconf/altoptions.m4 \
	$(NULL)

$(TOPSRCDIR)/configure: $(TOPSRCDIR)/configure.in $(EXTRA_CONFIG_DEPS)
	@echo Generating $@ using autoconf
	cd $(TOPSRCDIR); $(AUTOCONF)
endif

$(OBJDIR)/Makefile: $(TOPSRCDIR)/configure $(TOPSRCDIR)/allmakefiles.sh $(TOPSRCDIR)/.client-defs.mk
	@if test ! -d $(OBJDIR); then $(MKDIR) $(OBJDIR); else true; fi
	@echo cd $(OBJDIR); 
	@echo LD_LIBRARY_PATH=$(MOZ_WITH_NSPR)/lib:$(LD_LIBRARY_PATH) \\
	@echo ../configure $(CONFIG_FLAGS)
	@cd $(OBJDIR) && \
	  LD_LIBRARY_PATH=$(MOZ_WITH_NSPR)/lib:$(LD_LIBRARY_PATH) \
	  $(TOPSRCDIR)/configure $(CONFIG_FLAGS) \
	  || echo Fix above errors and then restart with \"$(MAKE) -f client.mk build\"

ifdef CONFIG_STATUS
$(OBJDIR)/config/autoconf.mk: $(TOPSRCDIR)/config/autoconf.mk.in
	cd $(OBJDIR); \
	  CONFIG_FILES=config/autoconf.mk ./config.status
endif

#
# Build it
#

build:  nspr $(OBJDIR)/Makefile 
	cd $(OBJDIR); $(MAKE);

# Build & install nspr.  Classic build, no autoconf.
# Linux/RPM available.
nspr:	$(NSPR_INSTALL_DIR)/lib/libnspr3.so
	@echo NSPR is installed in $(NSPR_INSTALL_DIR)/lib objdir=$(OBJDIR)

$(NSPR_INSTALL_DIR)/lib/libnspr3.so:
	@-$(MKDIR) -p $(NSPR_INSTALL_DIR)
	($(MAKE) -C $(TOPSRCDIR)/nsprpub $(NSPR_GMAKE_OPTIONS)) 

# NSPR is pulled by SeaMonkeyEditor module
#	cd $(ROOTDIR) && $(CVSCO) $(NSPR_BRANCH) NSPR

# Pass these target onto the real build system
clean realclean:
	cd $(OBJDIR); $(MAKE) $@;

# (! IS_FIRST_CHECKOUT)
endif
