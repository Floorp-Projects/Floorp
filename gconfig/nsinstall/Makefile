#! gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

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

