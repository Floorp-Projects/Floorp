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

MODULE		= xfeicons
LIBRARY_NAME	= xfeicons

CSRCS		= $(ICONS_GEN_SRC)
EXPORTS		= $(ICONS_GEN_HDR) icondata.h

REQUIRES	= img java jtools layer nspr util xp

LOCAL_INCLUDES	= -I. -I$(OBJDIR)

ICONS_GEN_HDR	= $(OBJDIR)/icon_extern.h
ICONS_GEN_SRC	= $(OBJDIR)/icondata.c

ANIM_PRIV = $(DEPTH)/../ns_priv/xfe/icons/anim

include $(DEPTH)/config/rules.mk

#### NEXT FEW LINES ARE ONLY FOR TESTING -- full build!
#MOZ_MAIL_NEWS	= 1
#EDITOR		= 1
#MOZ_OFFLINE	= 1
#MOZ_LOC_INDEP	= 1
#MOZ_TASKBAR	= 1
#MOZ_LDAP	= 1
#MOZ_ADMIN_LIB	= 1
#MOZ_COMMUNICATOR_NAME	= 1
#MOZ_JSD		= 1
#MOZ_IFC_TOOLS	= 1
#MOZ_NETCAST	= 1
#MOZ_COMMUNICATOR_IIDS	= 1
#MOZ_COMMUNICATOR_ABOUT	= 1
#MOZ_COMMUNICATOR_CONFIG_JS	= 1
#MOZ_COPY_ALL_JARS	= 1
#### END OF TESTING LINES

# The cc compiler on OSF1 V4 cannot handle icondata.c (runs out of
# swap space) unless we give the -oldc option.
ifeq ($(OS_ARCH)$(OS_RELEASE),OSF1V4)
CFLAGS		+= -oldc
endif

# See comment in icondata.c for why this is necessary.
DEFINES		+= -DXFE

# Split the icons by portion of the product.
# Be careful to avoid hitting max line length on Irix,
# therefore, split the icon list among several variables.
ICONS = $(wildcard images/[AGIHL]*.gif images/BM*.gif images/DTB*.gif)
ICONS += $(wildcard images/Dash*.gif images/TB*.gif)
ICONS += \
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

# Icons for the Outliner -- needed for Mail/News but also edit bookmarks
ICONS += \
	images/hidecolumn.gif \
	images/hidecolumn.i.gif \
	images/oparent.gif \
	images/showcolumn.gif \
	images/showcolumn.i.gif \
	images/threadoff.gif \
	images/threadon.gif

ICONS += $(wildcard images/M*.gif)
ICONS += \
	images/Desk_Address.gif \
	images/Desk_Collabra.gif \
	images/Desk_Messenger.gif \
	images/Desk_MsgCenter.gif \
	images/Desk_MsgCompose.gif \
	images/Desk_NewMail.gif

ifdef EDITOR
ICONS += $(wildcard images/e*.gif images/Desk_Composer.gif)
endif

# Should the taskbar icons be broken up by type?
ifdef MOZ_TASKBAR
ICONS += $(wildcard images/Task*.gif)
endif

ifdef MOZ_NETCAST
ICONS += images/Desk_Netcaster.gif
endif

# Alphabetical order seems to matter here.
ifdef NETSCAPE_PRIV
ANIM		+= $(wildcard $(ANIM_PRIV)/main/*.gif \
			$(ANIM_PRIV)/compass/*.gif \
			$(ANIM_PRIV)/mozilla/*.gif)
else
ANIM		= $(wildcard anim/main/*.gif)
endif

ICONLIST	 = $(OBJDIR)/icon.list
ICONANIMLIST = $(OBJDIR)/iconanim.list
MKICONS_EXE	 = $(OBJDIR)/mkicons

ifdef JAVA_OR_OJI
JAVA_JMC = $(DIST)/lib/libjmc.a        # XXX To be removed...
else
JAVA_JMC = $(DIST)/lib/libstubsj.a
endif

ifndef NSPR20
MKICONS_LIB	=  \
		  $(DIST)/lib/libimg.a \
		  $(JAVA_JMC) \
		  $(DIST)/lib/libpng.a \
		  $(DIST)/lib/libzlib.a \
		  /usr/lib/libm.a  \
		  $(DIST)/lib/libnspr.a \
		  $(DIST)/lib/libjpeg.a \
		  $(DIST)/lib/libutil.a \
		  $(DIST)/lib/libxp.a
else
MKICONS_LIB	=  \
		  $(DIST)/lib/libimg.a \
		  $(JAVA_JMC) \
		  $(DIST)/lib/libpng.a \
		  $(DIST)/lib/libzlib.a \
		  /usr/lib/libm.a  \
		  $(DIST)/lib/libnspr21.a \
		  $(DIST)/lib/libjpeg.a \
		  $(DIST)/lib/libutil.a \
		  $(DIST)/lib/libxp.a
endif

GARBAGE		+= $(MKICONS_EXE) \
		  $(ICONS_GEN_HDR) \
		  $(ICONS_GEN_HDR).tmp \
		  $(ICONS_GEN_SRC) \
		  $(ICONLIST) \
		  $(ICONANIMLIST)

MKICONS_EXTRA	= $(OS_LIBS)

#######################################################################
# Rules to build mkicons program and then icondata.c

# XXX We need to reach into the Image Library modules's source directory to 
# get the file if.h
$(MKICONS_EXE): mkicons.c $(MKICONS_LIB)
	$(CCF) -o $@ -I$(DEPTH)/modules/libimg/src $< $(MKICONS_LIB) $(MKICONS_EXTRA)

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
	@sed 's/^images\/\(.*\)\.gif.*/extern struct fe_icon_data \1;/; y/./_/;' \
		$(ICONLIST) > $@.$$$$ \
	&& if [ ! -f $(ICONS_GEN_HDR) -o "`cmp $@.$$$$ $@`" != "" ] ; then \
		echo Generating $@ from $(ICONLIST); \
		\mv -f $@.$$$$ $@; \
	fi

$(ICONS_GEN_SRC): icondata.h $(ICONANIMLIST) $(MKICONS_EXE) $(ICONS_GEN_HDR)
	$(MKICONS_EXE) -no-xfe-define - < $(ICONANIMLIST) > $@.$$$$ && mv $@.$$$$ $@

$(LIBRARY): $(OBJS)
