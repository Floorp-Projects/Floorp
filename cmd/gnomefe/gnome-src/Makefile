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

DEPTH		= ../../..

include $(DEPTH)/config/config.mk

MODULE		= gnuzilla

LIBRARY_NAME	= gnuzilla

REQUIRES	= nspr layer js jtools applet security img util parse layout hook pref java libfont ldap lay style xfeicons rdf xpcom plug mariner gnuzilla-widgets

CSRCS		= g-bookmark-frame.c \
		  g-bookmark-view.c \
		  g-browser-frame.c \
		  g-commands.c \
		  g-component.c \
		  g-editor-frame.c \
		  g-editor-view.c \
		  g-frame.c \
		  g-history-frame.c \
		  g-history-view.c \
		  g-html-forms.c \
		  g-html-view.c \
		  g-navcenter-view.c \
		  g-paned-view.c \
		  g-tagged.c \
		  g-util.c \
		  g-view.c \
	          g-font-select.c

EXPORTS		= g-bookmark-frame.h \
		  g-bookmark-view.h \
		  g-browser-frame.h \
		  g-commands.h \
		  g-component.h \
		  g-editor-frame.h \
		  g-editor-view.h \
		  g-frame.h \
		  g-history-frame.h \
		  g-history-view.h \
		  g-html-view.h \
		  g-navcenter-view.h \
		  g-paned-view.h \
		  g-tagged.h \
		  g-types.h \
		  g-util.h \
		  g-view.h \
		  g-font-select.h

GNOME_CFLAGS := $(shell gnome-config --cflags gnomeui)
INCLUDES	+= -I$(OBJDIR) -I.. -I. $(GNOME_CFLAGS)

include $(DEPTH)/config/rules.mk
