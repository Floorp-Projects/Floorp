#! gmake
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

DEPTH		= ../..
GDEPTH	= ../..

MODULE		= gconfig

CSRCS		= nsinstall.c pathsub.c

PLSRCS		= nfspwd.pl

PROGRAM		= nsinstall

include $(DEPTH)/gconfig/config.mk

ifeq ($(OS_ARCH),WINNT)
PROGRAM		=
else
TARGETS		= $(PROGRAM) $(PLSRCS:.pl=)
endif

include $(DEPTH)/gconfig/rules.mk

# Redefine MAKE_OBJDIR for just this directory
define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); fi
endef

