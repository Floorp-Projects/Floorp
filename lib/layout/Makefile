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

DEPTH		= ../..

MODULE		= lay
LIBRARY_NAME	= lay

EXPORTS		= layout.h lo_funcs.h

CSRCS		= bullet.c \
		  clipline.c \
		  layarena.c \
		  layblock.c \
		  laycell.c \
		  laycols.c \
		  laydisp.c \
		  layembed.c \
		  layfind.c \
		  layform.c \
		  laygrid.c \
		  layhrule.c \
		  layinfo.c \
		  laylist.c \
		  laymap.c \
		  layobj.c \
		  layscrip.c \
		  layspace.c \
		  layspan.c \
		  laystyle.c \
		  laysub.c \
		  ptinpoly.c \
		  laymocha.c \
		  layjava.c \
		  laylayer.c \
		  layrelay.c \
		  laytrav.c \
		  $(NULL)

REQUIRES	= parse style js layer applet nspr security dbm img util jtools edtplug java hook pref libfont lay rdf

include $(DEPTH)/config/config.mk

ifdef EDITOR
CSRCS		+= layedit.c \
		  layfree.c \
		  layimage.c \
		  layout.c \
		  laysel.c \
		  laytable.c \
		  laytags.c \
		  laytext.c \
		  layutil.c \
		  $(NULL)

CPPSRCS		= streams.cpp \
		  fsfile.cpp \
		  editor.cpp \
		  edtbuf.cpp \
		  edtcmd.cpp \
		  edtele.cpp \
		  edtjava.cpp \
		  edtsave.cpp \
		  edtutil.cpp \
		  $(NULL)
endif

include $(DEPTH)/config/rules.mk

ifneq ($(OS_ARCH),OS2)
$(OBJDIR)/laymocha.o: laymocha.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) -I$(DEPTH)/mocha/include $<
else
$(OBJDIR)/laymocha.o: laymocha.c
	@$(MAKE_OBJDIR)
	$(CC) -Fo$@ -c $(CFLAGS) -I$(DEPTH)/mocha/include $<
endif

$(LIBRARY): $(OBJS)
