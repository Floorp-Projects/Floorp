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

ifndef WITH_NSPR
WITH_NSPR	= /usr/local/nspr
endif

-include $(DEPTH)/config/config.mk

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
build:
	PWD=`pwd`
	(cd mozilla; $(AUTOCONF))
	if test ! -d mozilla/$(OBJDIR); then rm -rf mozilla/$(@OBJDIR); mkdir -D mozilla/$(OBJDIR); else true; fi
	(cd mozilla/$(OBJDIR); ../configure --with-nspr=$(WITH_NSPR) --enable-toolkit=$(MOZ_TOOLKIT))
	(cd mozilla/$(OBJDIR); gmake depend)
	(cd mozilla/$(OBJDIR); gmake)

# Do an autoconf build, this isn't working yet. -mcafee
#
#	if test ! -d mozilla/$(FOO); then (cd mozilla; $(MKDIR) $(AUTODIR)); fi
#	@echo cd mozilla/obj-`build/autoconf/config.guess`; ../configure --with-nspr=$(PWD)/$(DIST)


# Build & install nspr.  Classic build, no autoconf.
# Linux/RPM available.
nspr:
	($(MAKE) -C mozilla/nsprpub DIST=$(WITH_NSPR) NSDISTMODE=copy NS_USE_GCC=1 MOZILLA_CLIENT=1 NO_MDUPDATE=1 NS_USE_NATIVE= )

