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

#
# Build the Mozilla client.
#
# This needs CVSROOT set to work, e.g.,
#   setenv CVSROOT :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
# or
#   setenv CVSROOT :pserver:username%somedomain.org@cvs.mozilla.org:/cvsroot
# 
# Usage:
#
#  To checkout and build a new tree,
#      cvs checkout mozilla/client.mk
#      gmake -f mozilla/client.mk
#  To checkout (update) and build an existing tree,
#      gmake -f mozilla/client.mk
#  To only checkout, configure, or build,
#      gmake -f mozilla/client.mk [checkout] [configure] [build]
#
# See http://www.mozilla.org/unix/ for more information.
#
# Options:
#   NSPR_INSTALL_DIR     - nspr directory for configure --with-nspr=
#   USE_PTHREADS         - pthreads for nspr and configure
#   MOZ_OBJDIR           - destination Object Directory (relative path)
# also,
#   MOZ_CVS_FLAGS        - flags to pass to CVS
#   MOZ_CHECKOUT_FLAGS   - flags to pass after cvs co
#   MOZ_BRANCH           - default branch to checkout
#   MOZ_TOOLKIT          - toolkit for configure --enable-toolkit=

# Basic static variables
# 
CWD		:= $(shell pwd)
ifeq (mozilla, $(notdir $(CWD)))
ROOTDIR		:= $(shell dirname $(CWD))
TOPSRCDIR	:= $(CWD)
else
ROOTDIR		:= $(CWD)
TOPSRCDIR	:= $(CWD)/mozilla
endif


AUTOCONF	:= autoconf
TARGETS		:= export libs install
MKDIR		:= mkdir
SH		:= /bin/sh
ifndef MAKE
MAKE		:= gmake
endif

CONFIG_GUESS	:= $(wildcard $(TOPSRCDIR)/build/autoconf/config.guess)
ifndef CONFIG_GUESS
  IS_FIRST_CHECKOUT := 1
endif

ifdef MOZ_OBJDIR
  OBJDIR := $(MOZ_OBJDIR)
else
# Default objdir, e.g. mozilla/../obj-sparc-sun-solaris2.5.1
  OBJDIR :=
  ifdef CONFIG_GUESS
    OBJDIR := $(ROOTDIR)/obj-$(shell $(CONFIG_GUESS))
  endif
endif

# 
# Step 1: CVS
#
# Add new cvs-related flags here in the form
# ifdef MOZ_FLAGNAME
# 	CVS_FLAGNAME = -option $(CVS_FLAGNAME)
# else (optional)
#   CVS_FLAGNAME = -some -defaults
# endif
# then:
# - DOCUMENT THE NEW OPTION ABOVE!
# - Add $(CVS_FLAGNAME) to CVS_FLAGS at the bottom


# Basic CVS flags
ifdef MOZ_CVS_FLAGS
  CVS_CFLAGS := $(MOZ_CVS_FLAGS)
else
  CVS_CFLAGS := -q -z 3
endif

# Anything that we should use on all checkouts
ifdef MOZ_CHECKOUT_FLAGS
  CVS_COFLAGS := $(MOZ_CHECKOUT_FLAGS)
else
  CVS_COFLAGS := -P
endif

# The default branch tag
ifdef MOZ_BRANCH
  CVS_BRANCH_FLAGS := -r $(MOZ_BRANCH_FLAGS)
endif

CVS		:= cvs $(CVS_CFLAGS)
CVSCO		:= $(CVS) co $(CVS_COFLAGS) $(CVS_BRANCH_FLAGS)
CVSCO_LOGFILE	:= $(ROOTDIR)/cvsco.log

#
# Step 2: NSPR
#

ifeq ($(USE_PTHREADS), 1)
NSPR_PTHREAD_FLAG := USE_PTHREADS=1
endif

NSPR_BRANCH :=
#NSPR_BRANCH := -rNSPRPUB_RELEASE_3_0

# These options can be overriden by the user

ifndef NSPR_INSTALL_DIR
NSPR_INSTALL_DIR	:= $(TOPSRCDIR)/$(OBJDIR)/nspr
# Need to build nspr since none was given
OPTIONAL_NSPR_BUILD	:= nspr
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
# Add new autoconf/configure flags here in the form:
# ifdef MOZ_FLAGNAME
#   CONFIG_FLAGNAME_FLAG = --some-config-option=$(MOZ_FLAGNAME)
# endif
# then:
# - DOCUMENT THE NEW OPTION ABOVE!
# - Add $(CONFIG_FLAGNAME_FLAG) to CONFIG_FLAGS at the bottom

# Default object directory, e.g. obj-sparc-sun-solaris2.5.1

ifdef MOZ_TOOLKIT
  CONFIG_TOOLKIT_FLAG	:= --enable-toolkit=$(MOZ_TOOLKIT)
endif

ifeq ($(USE_PTHREADS), 1)
  CONFIG_PTHREAD_FLAG	:= --with-pthreads
endif

CONFIG_FLAGS := \
	--with-nspr=$(NSPR_INSTALL_DIR) \
	--enable-editor \
	--enable-debug \
	$(CONFIG_PTHREADS_FLAG) \
	$(CONFIG_TOOLKIT_FLAG) \
	$(NULL)

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
	fi
	@date > $(CVSCO_LOGFILE)
	cd $(ROOTDIR) && \
	  $(CVSCO) SeaMonkeyEditor | tee -a $(CVSCO_LOGFILE)
	@if egrep -q "^C " $(CVSCO_LOGFILE) ; then \
	  echo "$(MAKE): *** Conflicts during checkout. \
	       Refer to $(CVSCO_LOGFILE)."; \
	  exit 1; \
	fi


ifdef IS_FIRST_CHECKOUT
build:
	$(MAKE) -f $(TOPSRCDIR)/client.mk $(OPTIONAL_NSPR_BUILD) build
else

#
# Configure
#

CONFIG_STATUS := $(wildcard $(OBJDIR)/config.status)
CONFIG_CACHE  := $(wildcard $(OBJDIR)/config.cache)

ifdef RUN_AUTOCONF_LOCALLY
EX_CONFIG_DEPS := \
	$(TOPSRCDIR)/aclocal.m4 \
	$(TOPSRCDIR)/build/autoconf/gtk.m4 \
	$(TOPSRCDIR)/build/autoconf/altoptions.m4 \
	$(NULL)

$(TOPSRCDIR)/configure: $(TOPSRCDIR)/configure.in $(EX_CONFIG_DEPS)
	@echo Generating $@ using autoconf
	cd $(TOPSRCDIR); $(AUTOCONF)
endif

$(OBJDIR)/Makefile:  $(TOPSRCDIR)/configure
	@if test ! -d $(OBJDIR); then $(MKDIR) $(OBJDIR); fi
	@echo cd $(OBJDIR); 
	@echo ../configure $(CONFIG_FLAGS)
	@cd $(OBJDIR) && \
	  LD_LIBRARY_PATH=$(NSPR_INSTALL_DIR)/lib:$(LD_LIBRARY_PATH) \
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

build:	$(OBJDIR)/Makefile
	cd $(OBJDIR); $(MAKE);

# Build & install nspr.  Classic build, no autoconf.
# Linux/RPM available.
nspr:	$(NSPR_INSTALL_DIR)/lib/libnspr21.so
	@echo NSPR is ready and installed in $(NSPR_INSTALL_DIR)

$(NSPR_INSTALL_DIR)/lib/libnspr21.so:
	@-$(MKDIR) -p $(NSPR_INSTALL_DIR)
	($(MAKE) -C $(TOPSRCDIR)/nsprpub $(NSPR_GMAKE_OPTIONS)) 

#	cd $(ROOTDIR) && $(CVSCO) $(NSPR_BRANCH) NSPR

# Pass these target onto the real build system
clean realclean:
	cd $(OBJDIR); $(MAKE) $@;

# (! IS_FIRST_CHECKOUT)
endif
