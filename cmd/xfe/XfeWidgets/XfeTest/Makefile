#!gmake
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


##########################################################################
#
# Name:			Makefile
# Description:	Makefile for XfeTest library
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################

DEPTH			= ../../../..

# XfeTest headers (only one) are exported to dist/include/Xfe
MODULE			= XfeTest/Xfe
INCL_SUBDIR	= /Xfe

LIBRARY_NAME	= XfeTest

CSRCS			= \
				TestAnim.c \
				TestApp.c \
				TestCallbacks.c \
				TestCreate.c \
				TestIcons.c \
				TestMenu.c \
				TestPixmap.c \
				TestToolBar.c \
				TestUtil.c \
				$(NULL)

REQUIRES		= \
				XfeTest \
				XfeWidgets \
				XfeBm \
				XfeToolBar \
				XfeComboBox

EXPORTS			= \
				XfeTest.h \
				$(NULL)

include $(DEPTH)/config/rules.mk

include $(DEPTH)/cmd/xfe/XfeWidgets/XfeWidgets.mk

INCLUDES	+= -I../icons
