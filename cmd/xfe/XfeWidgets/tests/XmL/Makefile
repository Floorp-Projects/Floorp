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

##########################################################################
#
# Name:			Makefile
# Description:	Makefile for Microline widgets test programs.
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################

DEPTH			= ../../../../..

CSRCS			= \
				TreeTest.c \
				$(NULL)

REQUIRES		= \
				XfeWidgets \
				XfeTest \
				Microline

STATIC_PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=.static))
SHARED_PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=.shared))

include $(DEPTH)/config/rules.mk
include $(DEPTH)/cmd/xfe/XfeWidgets/XfeWidgets.mk

all:: $(STATIC_PROGS) $(SHARED_PROGS)
install:: $(STATIC_PROGS) $(SHARED_PROGS)

shared: $(SHARED_PROGS)
static: $(STATIC_PROGS)
