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
include $(DEPTH)/cmd/unixfe/config/unixfe.mk

MODULE		= xfeicons
LIBRARY_NAME	= xfeicons

EXPORTS		= icondata.h

REQUIRES	= img java jtools layer nspr util xp xpcom \
		  $(XFE_JPEG_REQUIRES) $(XFE_PNG_REQUIRES) $(XFE_ZLIB_REQUIRES)

LOCAL_INCLUDES	= -I. -I$(OBJDIR)

ICONS_GEN_HDR	= $(OBJDIR)/icon_extern.h
ICONS_GEN_SRC	= $(OBJDIR)/icondata.c

include $(DEPTH)/config/rules.mk

OBJS		= $(ICONS_GEN_SRC:.c=.o)

# See comment in icondata.c for why this is necessary.
DEFINES		+= -DXFE

# Split the icons by portion of the product.
# Be careful to avoid hitting max line length on Irix,
# therefore, split the icon list among several variables.
ICONS		= $(wildcard images/[AGIHL]*.gif images/BM*.gif images/DTB*.gif)
ICONS		+= $(wildcard images/Dash*.gif images/TB*.gif)
ICONS		+= \
		   images/Desk_AdminKit.gif \
		   images/Desk_Bookmark.gif \
		   images/Desk_Calendar.gif \
		   images/Desk_Communicator.gif \
		   images/Desk_Conference.gif \
		   images/Desk_History.gif \
		   images/Desk_Navigator.gif \
		   images/Desk_Search.gif \
		   images/SEC_Replace.gif \
		   images/Splash.gif \
		   images/cparent.gif

# Icons for the Outliner
ICONS		+= $(wildcard images/hidecolumn*.gif images/showcolumn*.gif) \
		   images/oparent.gif

# Icons referenced in rosetta.h
ICONS		+= $(wildcard images/M_*.gif)

MAIL_OR_COMPOSE_ICONS	= $(wildcard images/MNC*.gif images/MNTB_Next*.gif \
			    images/MNTB_Forward*.gif images/MNAB_NewPerson*.gif \
			    images/MN_Draft*.gif images/MN_Collect*.gif \
			    images/MN_Template*.gif) \
			  images/MN_MailAttach.gif \
			  images/MN_Person.gif \
			  images/MN_People.gif

ifdef MOZ_MAIL_NEWS
ICONS		+= $(MAIL_OR_COMPOSE_ICONS) \
		   $(wildcard images/MNTB*.gif images/MNAB*.gif images/MN_*.gif) \
		   images/threadoff.gif \
		   images/threadon.gif \
		   images/Desk_Address.gif \
		   images/Desk_Collabra.gif \
		   images/Desk_Messenger.gif \
		   images/Desk_MsgCenter.gif \
		   images/Desk_NewMail.gif
endif

ifdef MOZ_MAIL_COMPOSE
ifndef MOZ_MAIL_NEWS
ICONS		+= $(MAIL_OR_COMPOSE_ICONS)
endif
ICONS		+= images/Desk_MsgCompose.gif
endif

ifdef MOZ_EDITOR
ICONS		+= $(wildcard images/e*.gif images/Desk_Composer.gif)
endif

ifdef MOZ_TASKBAR
ICONS		+= $(wildcard images/Task*.gif)
endif

ifdef MOZ_NETCAST
ICONS		+= images/Desk_Netcaster.gif
endif

ifdef NETSCAPE_COMMERCIAL
ANIM		= $(wildcard $(XFEPRIVDIR)/icons/anim/main/*.gif)
else
ANIM		= $(wildcard anim/main/*.gif)
endif

ICONLIST	= $(OBJDIR)/icon.list
ICONANIMLIST	= $(OBJDIR)/iconanim.list
MKICONS_EXE	= $(OBJDIR)/mkicons

ifdef JAVA_OR_NSJVM
JAVA_JMC	= $(DIST)/lib/libjmc.a        # XXX To be removed...
else
JAVA_JMC	= $(DIST)/lib/libstubsj.a
endif

MKICONS_LIB	= \
		  $(DIST)/lib/libimg.a \
		  $(JAVA_JMC) \
		  $(DIST)/lib/libnspr21.a \
		  $(DIST)/lib/libutil.a \
		  $(DIST)/lib/libxp.a \
		  $(DIST)/lib/libplc21.a \
		  $(DIST)/lib/libxpcom.a \
		  $(XFE_MKICONS_PNG_LIB) \
		  $(XFE_MKICONS_ZLIB_LIB) \
		  $(XFE_MKICONS_JPEG_LIB) \
		  $(NULL)

MKICONS_DSOS	= \
		  $(XFE_MKICONS_JPEG_DSO) \
		  $(XFE_MKICONS_PNG_DSO) \
		  $(XFE_MKICONS_ZLIB_DSO) \
		  $(NULL)

GARBAGE		+= $(MKICONS_EXE) \
		  $(wildcard $(ICONS_GEN_HDR)*) \
		  $(wildcard $(ICONS_GEN_SRC)*) \
		  $(ICONLIST) \
		  $(ICONANIMLIST)

MKICONS_EXTRA	= $(MKICONS_DSOS) $(OS_LIBS) -lm

#######################################################################
# Rules to build mkicons program and then icondata.c

# XXX We need to reach into the Image Library modules's source directory to 
# get the file if.h
$(MKICONS_EXE): mkicons.cpp $(MKICONS_LIB)
	$(CCC) $(CFLAGS) -o $@ -I$(DEPTH)/modules/libimg/src $< $(XFE_MKICONS_BIN_LDPATH) $(MKICONS_LIB) $(MKICONS_EXTRA)

$(ICONLIST): $(ICONS)
	@echo Adding icons to $@
	@$(MAKE_OBJDIR)
	@rm -f $@
	@ls $(ICONS) > $@

$(ICONANIMLIST): $(ICONLIST) $(ANIM)
	@echo Adding icons and animations to $(ICONANIMLIST)
	@rm -f $(ICONANIMLIST)
	@cat $(ICONLIST) > $(ICONANIMLIST)
ifneq ($(ANIM),)
	@ls $(ANIM) >> $(ICONANIMLIST)
endif

$(ICONS_GEN_HDR): $(ICONLIST)
	@rm -f $@.tmp
	@sed 's/^images\/\(.*\)\.gif.*/extern struct fe_icon_data \1;/; y/./_/;' $(ICONLIST) > $@.tmp
	@if test ! -f $@; then touch $@; else true; fi
	@if test ! -z "`diff $@.tmp $@`"; then rm -f $@; mv $@.tmp $@; else true; fi

$(ICONS_GEN_SRC): icondata.h $(ICONANIMLIST) $(MKICONS_EXE) $(ICONS_GEN_HDR)
	@rm -f $@
	$(MKICONS_EXE) -no-xfe-define - < $(ICONANIMLIST) > $@

$(LIBRARY): $(OBJS)

export:: $(ICONS_GEN_HDR) $(DIST)/include
	$(INSTALL) -m 444 $^

