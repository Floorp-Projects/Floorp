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
# Description:	Makefile for XfeToolBar library
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################


DEPTH			= ../../../..

# XfeWidgets headers are exported to dist/include/Xfe.
MODULE			= XfeToolBar/Xfe
INCL_SUBDIR	= /Xfe

LIBRARY_NAME	= XfeToolBar

CSRCS			= \
				Chrome.c \
				DashBoard.c \
				Tab.c \
				ToolBar.c \
				ToolBox.c \
				ToolItem.c \
				ToolScroll.c \
				$(NULL)

REQUIRES		= \
				XfeToolBar \
				XfeComboBox \
				XfeWidgets

EXPORTS			= \
				Chrome.h \
				ChromeP.h \
				DashBoard.h \
				DashBoardP.h \
				Tab.h \
				TabP.h \
				ToolBar.h \
				ToolBarP.h \
				ToolBox.h \
				ToolBoxP.h \
				ToolItem.h \
				ToolItemP.h \
				ToolScroll.h \
				ToolScrollP.h \
				$(NULL)

include $(DEPTH)/config/rules.mk
