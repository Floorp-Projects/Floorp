#!gmake
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
# Description:	Makefile for XfeBm library.
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################

DEPTH			= ../../../..

# XfeWidgets headers are exported to dist/public/XfeWidgets/Xfe
MODULE			= XfeWidgets/Xfe

LIBRARY_NAME	= XfeBm

CSRCS			= \
				BmAccent.c \
				BmButton.c \
				BmCascade.c \
				BmGlobal.c \
				BmRepType.c \
				$(NULL)

REQUIRES		=\
				XfeWidgets \
				XfeBm

EXPORTS			= \
				BmButton.h \
				BmButtonP.h \
				BmCascade.h \
				BmCascadeP.h \
				XfeBm.h \
				$(NULL)

include $(DEPTH)/config/rules.mk
