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
# This needs CVSROOT set to work, e.g.
# setenv CVSROOT :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
#  --or--
# setenv CVSROOT :pserver:username%somedomain.org@cvs.mozilla.org:/cvsroot
# 
# Usage:
# Pull the source:
#   cvs checkout mozilla/client.mk
#   gmake -f mozilla/client.mk checkout
#
# Build NSPR, maybe only once:
#   gmake -f mozilla/client.mk nspr
#
# Build the client:
#   gmake -f mozilla/client.mk build
#
# see http://www.mozilla.org/unix/ for more information

# options:
# MOZ_OBJDIR		 - destination Object Directory
# MOZ_CVS_FLAGS 	 - flags to pass to CVS
# MOZ_CHECKOUT_FLAGS - flags to pass after cvs co
# MOZ_BRANCH		 - default branch to checkout
# MOZ_TOOLKIT		 - toolkit for configure --enable-toolkit=
# NSPR_INSTALL_DIR	 - nspr directory for configure --with-nspr=

#
# basic static variables
# 
DEPTH		= mozilla
AUTOCONF	= autoconf -l build/autoconf
TARGETS		= export libs install
MKDIR		= mkdir
SH			= /bin/sh
CWD			= $(shell pwd)
SRCDIR		= $(CWD)/$(DEPTH)

ifndef MAKE
MAKE		= gmake
endif

# default objdir, e.g. obj-sparc-sun-solaris2.5.1
ifndef MOZ_OBJDIR
  OBJDIR = $(CWD)/obj-$(shell $(SRCDIR)/build/autoconf/config.guess)
else
  OBJDIR = $(MOZ_OBJDIR)
endif

# 
# Step 1: CVS
#
# add new cvs-related flags here in the form
# ifdef MOZ_FLAGNAME
# 	CVS_FLAGNAME = -option $(CVS_FLAGNAME)
# else (optional)
#   CVS_FLAGNAME = -some -defaults
# endif
# then:
# - DOCUMENT THE NEW OPTION ABOVE!
# - add $(CVS_FLAGNAME) to CVS_FLAGS at the bottom


# basic CVS flags
ifdef MOZ_CVS_FLAGS
  CVS_CFLAGS = $(MOZ_CVS_FLAGS)
else
  CVS_CFLAGS = -q -z 3
endif

# anything that we should use on all checkouts
ifdef MOZ_CHECKOUT_FLAGS
  CVS_COFLAGS = $(MOZ_CHECKOUT_FLAGS)
else
  CVS_COFLAGS = -P
endif

# the default branch tag
ifdef MOZ_BRANCH
  CVS_BRANCH_FLAGS = -r $(MOZ_BRANCH_FLAGS)
endif

CVS			= cvs $(CVS_CFLAGS)
CVSCO		= $(CVS) co $(CVS_COFLAGS) $(CVS_BRANCH_FLAGS)

#
# Step 2: NSPR
#

# these options can be overriden by the user
ifndef NSPR_INSTALL_DIR
NSPR_INSTALL_DIR = $(OBJDIR)/nspr
endif

ifndef NSPR_OPTIONS
NSPR_OPTIONS = NS_USE_GCC=1 NS_USE_NATIVE=
endif

# these options are required to make this makefile build NSPR correctly
NSPR_REQ_OPTIONS = MOZILLA_CLIENT=1 NO_MDUPDATE=1
NSPR_DIST_OPTION = DIST=$(NSPR_INSTALL_DIR) NSDISTMODE=copy
NSPR_TARGET = export

NSPR_GMAKE_OPTIONS = $(NSPR_DIST_OPTION) $(NSPR_REQ_OPTIONS) $(NSPR_OPTIONS) $(NSPR_TARGET)

#
# Step 3: autoconf
#
# add new autoconf/configure flags here in the form:
# ifdef MOZ_FLAGNAME
#   CONFIG_FLAGNAME_FLAG = --some-config-option=$(MOZ_FLAGNAME)
# endif
# then:
# - DOCUMENT THE NEW OPTION ABOVE!
# - add $(CONFIG_FLAGNAME_FLAG) to CONFIG_FLAGS at the bottom

# default object directory, e.g. obj-sparc-sun-solaris2.5.1

ifdef MOZ_TOOLKIT
  CONFIG_TOOLKIT_FLAG = --enable-toolkit=$(MOZ_TOOLKIT)
endif

CONFIG_NSPR_FLAG = --with-nspr=$(NSPR_INSTALL_DIR)

# Enable editor ...  might want to do this conditionally
CONFIG_EDITOR_FLAG = --enable-editor

# Turn on debug:
DEBUG_FLAG = --enable-debug

CONFIG_FLAGS = $(CONFIG_TOOLKIT_FLAG) $(CONFIG_NSPR_FLAG) $(CONFIG_EDITOR_FLAG)

#
# rules
# 

all: checkout

.PHONY: checkout

#
# CVS checkout
#
checkout:
# Build the client.
	$(CVSCO) SeaMonkeyEditor

#
# build it
#
# configure.in -> configure using autoconf
$(SRCDIR)/configure: $(SRCDIR)/configure.in
	@echo Generating $@ using autoconf
	(cd $(SRCDIR); $(AUTOCONF))

# configure -> Makefile by running 'configure'
$(OBJDIR)/Makefile: $(SRCDIR)/configure
	@echo Determining configuration to generate $@
	-$(MKDIR) $(OBJDIR)
	(cd $(OBJDIR) ; LD_LIBRARY_PATH=$(NSPR_INSTALL_DIR)/lib:$(LD_LIBRARY_PATH) $(SRCDIR)/configure $(CONFIG_FLAGS))

build:	$(OBJDIR)/Makefile
	(cd $(OBJDIR); $(MAKE));


# Build & install nspr.  Classic build, no autoconf.
# Linux/RPM available.
nspr:	$(NSPR_INSTALL_DIR)/lib/libnspr21.so
	@echo NSPR is ready and installed in $(NSPR_INSTALL_DIR)

$(NSPR_INSTALL_DIR)/lib/libnspr21.so:
	$(CVSCO) -rNSPRPUB_RELEASE_3_0 NSPR
	@-$(MKDIR) -p $(NSPR_INSTALL_DIR)
	($(MAKE) -C $(SRCDIR)/nsprpub $(NSPR_GMAKE_OPTIONS)) 

