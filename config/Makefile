# -*- Mode: Makefile -*-
#
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

DEPTH		= ..

HSRCS		= pathsub.h
CSRCS		= nsinstall.c pathsub.c

ifeq ($(subst /,_,$(shell uname -s)),OS2)
DIRS		= os2
LOCAL_INCLUDES	+= -Ios2
HSRCS		+= dirent.h getopt.h
endif

PLSRCS		= nfspwd.pl revdepth.pl

TARGETS		= $(PROGRAM) $(PLSRCS:.pl=)

ifneq ($(subst /,_,$(shell uname -s)),WINNT)
PROGRAM		= nsinstall$(BIN_SUFFIX)
TARGETS		+= $(OBJDIR)/bsdecho$(BIN_SUFFIX)
endif

# IMPORTANT: Disable NSBUILDROOT for this directory only, otherwise we have
# a recursive rule for finding nsinstall and the perl scripts
ifdef NSBUILDROOT
override NSBUILDROOT :=
endif

include $(DEPTH)/config/rules.mk

ifeq ($(OS_ARCH)$(OS_RELEASE),SunOS4.1)
INCLUDES	+= -I../nsprpub/pr/include/md
endif

ifeq ($(OS_ARCH)$(OS_RELEASE),IRIX5)
TARGETS		+= $(OBJDIR)/gtscc$(BIN_SUFFIX)
endif

# On linux we need to generake a motif.mk file which has special flags
# for different motif versions and/or broken libraries.
#
# Currently there is only 2 flags:
#
# MOZILLA_XM_2_1_PRINT_SHELL_FLAGS: X Print Shell Extension available
# starting with X11R6.3 and needed by motif 2.1.
#
# MOZILLA_XM_2_1_BROKEN_LOCALE_FLAGS: Needed because of currently
# broken locale support in motif 2.1 when linked with gnu libc2.
#
# Please direct questions about motif and linux to ramiro@netscape.com.
#
ifeq ($(OS_ARCH),Linux)

export:: motif.mk

motif.mk:
	@rm -f $@

# Check for motif 2.1 which needs the X Print Shell extension (-lXp)
#
# Also check for /lib/libc.so.6 (GNU libc2).  If we are using GLIBC2 with
# Motif 2.1, we need to check for /usr/lib/libBrokenLocale.so.  At this 
# time locale support on motif 2.1 seems to break with GLIBC2.
ifeq ($(shell $(DEPTH)/config/xmversion.sh),2.1)

	@echo "MOZILLA_XM_2_1_PRINT_SHELL_FLAGS = -lXp" > $@

	@if test -f /lib/libc.so.6 -a -f /usr/lib/libBrokenLocale.so; then \
	echo "MOZILLA_XM_2_1_BROKEN_LOCALE_FLAGS = -lBrokenLocale" >> $@; \
	fi
else
	@echo "" > $@
endif

endif

# Redefine MAKE_OBJDIR for just this directory
define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); fi
endef

export:: $(TARGETS)

ifeq ($(OS_ARCH),OS2)
# could not get bsdecho rules to work implicitly, so here is explicit ones
$(OBJDIR)/bsdecho.o:  bsdecho.c
	@$(MAKE_OBJDIR)
	$(CC) -Fo$@ $(CFLAGS) -c $<

$(OBJDIR)/bsdecho$(BIN_SUFFIX):	$(OBJDIR)/bsdecho.o
	@$(MAKE_OBJDIR)
	$(LINK_EXE) -OUT:$@ $< $(LDFLAGS)
	$(INSTALL) -m 444 $@ $(DIST)/bin
else
$(OBJDIR)/bsdecho$(BIN_SUFFIX):	$(OBJDIR)/bsdecho.o
	@$(MAKE_OBJDIR)
	$(CCF) $(LDFLAGS) -o $@ $<
	$(INSTALL) -m 444 $@ $(DIST)/bin
endif

$(OBJDIR)/gtscc$(BIN_SUFFIX):	$(OBJDIR)/gtscc.o
	@$(MAKE_OBJDIR)
	$(CCF) $(LDFLAGS) -o $@ $< -lelf
	$(INSTALL) -m 444 $@ $(DIST)/bin

ifdef MKDEPEND_DIR
clean clobber realclean clobber_all::
	cd $(MKDEPEND_DIR); $(MAKE) $@
endif

# For the continuous build scripts.
show_objname:
	@echo $(OBJDIR)

.PHONY: show_objname
