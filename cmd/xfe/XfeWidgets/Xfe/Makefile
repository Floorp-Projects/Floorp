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
# Description:	Makefile for XfeWidgets library
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################

DEPTH			= ../../../..

# XfeWidgets headers are exported to dist/public/XfeWidgets/Xfe
MODULE			= XfeWidgets/Xfe

LIBRARY_NAME	= XfeWidgets

# The XfeTempTwo widget is a demo.  It has all the possible bells and
# whistles for a subclass of XfeManager.  It can be used as a template
# when creating new XfeManager widgets.  Its not built by default
ifdef XFE_WIDGETS_BUILD_DEMO

XFE_EXTRA_DEFINES += -DXFE_WIDGETS_BUILD_DEMO

DEMO_CSRCS		= \
				TempTwo.c \
				$(NULL)

DEMO_EXPORTS	= \
				TempTwo.h \
				TempTwoP.h \
				$(NULL)
endif

# There are unused widgets.  They are currently not needed to build Mozilla.
# Some of them are works in progress for new features.  Some of them are
# ideas partially implemented, but never used.  Most of them are likely to
# be used in the future in some form.  They also might contain usefull code,
# so leave them alone.
ifdef XFE_WIDGETS_BUILD_UNUSED

XFE_EXTRA_DEFINES += -DXFE_WIDGETS_BUILD_UNUSED

UNUSED_CSRCS	= \
				ComboBox.c \
				FancyBox.c \
				FontChooser.c \
				$(NULL)

UNUSED_EXPORTS	= \
				ComboBox.h \
				ComboBoxP.h \
				FancyBox.h \
				FancyBoxP.h \
				FontChooser.h \
				FontChooserP.h \
				$(NULL)
endif


CSRCS			= \
				$(DEMO_CSRCS) \
				$(UNUSED_CSRCS) \
				Arrow.c \
				BmButton.c \
				BmCascade.c \
				Button.c \
				BypassShell.c \
				CallProcs.c \
				Cascade.c \
				ChildrenUtil.c \
				Chrome.c \
				Converters.c \
				Cursor.c \
				DashBoard.c \
				Debug.c \
				DialogUtil.c \
				Draw.c \
				Find.c \
				FrameShell.c \
				GcUtil.c \
				Geometry.c \
				Label.c \
				ListUtil.c \
				Logo.c \
				Manager.c \
				MenuUtil.c \
				Oriented.c \
				Pane.c \
				PixmapUtil.c \
				Primitive.c \
				Private.c \
				ProgressBar.c \
				RepType.c \
				Resources.c \
				ShellUtil.c \
				StringUtil.c \
				Tab.c \
				TaskBar.c \
				ToolBar.c \
				ToolBox.c \
				ToolItem.c \
				ToolScroll.c \
				Translations.c \
				Util.c \
				WmUtil.c \
				$(NULL)

REQUIRES		= XfeWidgets

EXPORTS			= \
				$(DEMO_EXPORTS) \
				$(UNUSED_EXPORTS) \
				Arrow.h \
				ArrowP.h \
				BmButton.h \
				BmButtonP.h \
				BmCascade.h \
				BmCascadeP.h \
				Button.h \
				ButtonP.h \
				BypassShell.h \
				BypassShellP.h \
				Cascade.h \
				CascadeP.h \
				ChildrenUtil.h \
				Chrome.h \
				ChromeP.h \
				Converters.h \
				DashBoard.h \
				DashBoardP.h \
				Debug.h \
				Defaults.h \
				DialogUtil.h \
				Draw.h \
				Find.h \
				FrameShell.h \
				FrameShellP.h \
				Geometry.h \
				GeometryP.h \
				Label.h \
				LabelP.h \
				ListUtil.h \
				ListUtilP.h \
				Logo.h \
				LogoP.h \
				Manager.h \
				ManagerP.h \
				MenuUtil.h \
				Oriented.h \
				OrientedP.h \
				PrepareP.h \
				Pane.h \
				PaneP.h \
				Primitive.h \
				PrimitiveP.h \
				ProgressBar.h \
				ProgressBarP.h \
				RepType.h \
				Resources.h \
				ShellUtil.h \
				StringDefs.h \
				StringUtil.h \
				Tab.h \
				TabP.h \
				TaskBar.h \
				TaskBarP.h \
				ToolBar.h \
				ToolBarP.h \
				ToolBox.h \
				ToolBoxP.h \
				ToolItem.h \
				ToolItemP.h \
				ToolScroll.h \
				ToolScrollP.h \
				WmUtil.h \
				Xfe.h \
				XfeAll.h \
				XfeP.h \
				$(NULL)

include $(DEPTH)/config/rules.mk

DEFINES		+= $(XFE_EXTRA_DEFINES)
