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
