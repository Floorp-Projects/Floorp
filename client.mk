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
# setenv CVSROOT :pserver:$(USER)%netscape.com@cvs.mozilla.org:/cvsroot
# 
# Usage:
# Pull the source:
#   cvs update mozilla/client.mk
#   gmake -f mozilla/client.mk checkout
#
# Build NSPR, maybe only once:
#   gmake -f mozilla/client.mk nspr
#
# Build the client:
#   gmake -f mozilla/client.mk build
#

DEPTH=mozilla

# Allow for cvs flags
ifndef CVS_FLAGS
CVS_CFLAGS = -q -z 3
endif

CVSCO		= cvs $(CVS_FLAGS) co -P
MAKE		= gmake
AUTOCONF	= autoconf
TARGETS		= export libs install
MKDIR		= mkdir
SH			= /bin/sh

ifndef MOZ_TOOLKIT
MOZ_TOOLKIT	= USE_DEFAULT
endif

ifndef NSPR_INSTALL_DIR
NSPR_INSTALL_DIR = /usr/local/nspr
NSPR_CONFIG_FLAG = --with-nspr=$(NSPR_INSTALL_DIR)
else
NSPR_CONFIG_FLAG = 
endif

all: checkout

.PHONY: checkout

# List branches here.
#

checkout:
# Pull the core layout stuff.
	$(CVSCO) mozilla/nglayout.mk
	(cd mozilla; $(MAKE) -f nglayout.mk pull_all)

# Pull xpfe
	$(CVSCO) mozilla/xpfe


# Build with autoconf
configure:	mozilla/configure.in
	autoobjdir=obj-$(shell mozilla/build/autoconf/config.guess); \
	echo autoobjdir = $$autoobjdir; \
	(cd mozilla; $(AUTOCONF)); \
	if test ! -d mozilla/$$autoobjdir; then $(MKDIR) mozilla/$$autoobjdir; fi; \
	(cd mozilla/$$autoobjdir; ../configure $(NSPR_CONFIG_FLAG) --enable-toolkit=$(MOZ_TOOLKIT)); \

build:	configure
	(cd mozilla/$$autoobjdir; $(MAKE)); \
#	(cd mozilla/$$autoobjdir; $(MAKE) depend); \

# Build & install nspr.  Classic build, no autoconf.
# Linux/RPM available.
nspr:	$(NSPR_INSTALL_DIR)/lib/libnspr21.so
$(NSPR_INSTALL_DIR)/lib/libnspr21.so:
	($(MAKE) -C mozilla/nsprpub DIST=$(NSPR_INSTALL_DIR) NSDISTMODE=copy NS_USE_GCC=1 MOZILLA_CLIENT=1 NO_MDUPDATE=1 NS_USE_NATIVE= )

