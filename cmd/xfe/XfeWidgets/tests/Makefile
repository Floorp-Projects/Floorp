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


# 

DEPTH			= ../../../..

ifdef XFE_WIDGETS_BUILD_UNUSED

XFE_EXTRA_DEFINES += -DXFE_WIDGETS_BUILD_UNUSED

UNUSED_CSRCS		= \
				ArrowTest.c \
				ChromeTest.c \
				ComboBoxTest.c \
				FontChooserTest.c \
				FrameShellTest.c \
				$(NULL)


endif

CSRCS			= \
				$(UNUSED_CSRCS) \
				ButtonTest.c \
				CascadeTest.c \
				DashBoardTest.c \
				LabelTest.c \
				LogoTest.c \
				MenuTest.c \
				PaneTestOne.c \
				PaneTestThree.c \
				PaneTestTwo.c \
				PixmapTest.c \
				ProgressBarTest.c \
				TaskBarTestOne.c \
				TaskBarTestTwo.c \
				ToolBarTest.c \
				ToolBoxTest.c \
				ToolScrollTest.c \
				$(NULL)

REQUIRES		= \
				XfeTest \
				XfeWidgets

PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=.exe))

include $(DEPTH)/config/rules.mk

DEFINES		+= $(XFE_EXTRA_DEFINES)

all:: $(PROGS)
install:: $(PROGS)

LDFLAGS =

ifeq ($(OS_ARCH),HP-UX)

  SHARED_EXT = sl

else

  SHARED_EXT = so

endif

SHARED_XFE_LIBS = \
				$(DIST)/bin/libXfeTest.$(SHARED_EXT) \
				$(DIST)/bin/libXfeWidgets.$(SHARED_EXT)

ifeq (,$(filter Linux IRIX ,$(OS_ARCH)))

SHARED_XFE_LIBS += $(DIST)/bin/libXpm.$(SHARED_EXT)
STATIC_XFE_LIBS += $(DIST)/lib/libXpm.a

else

XPM_LD_FLAGS = -lXpm

endif

STATIC_XFE_LIBS = \
				$(DIST)/lib/libXfeTest.a \
				$(DIST)/lib/libXfeWidgets.a

DIST_FLAGS = -L$(DIST)/bin

ifeq ($(OS_ARCH),AIX)

  XFE_FLAGS	= $(STATIC_XFE_LIBS)

else

  XFE_FLAGS	= $(SHARED_XFE_LIBS)

endif

ifeq ($(OS_ARCH),Linux)
-include $(DEPTH)/config/motif.mk
endif

XM_LD_FLAGS = -lXm $(NS_MOTIF2_XP_LD_FLAGS)

X_LD_FLAGS = $(XM_LD_FLAGS) -lXt -lXmu $(XPM_LD_FLAGS) -lXext -lX11

OS_BEFORE_FLAGS =
OS_AFTER_FLAGS =

ifeq ($(OS_ARCH),SunOS)

OS_BEFORE_LDFLAGS =\
			-L/usr/dt/lib \
			-L/usr/openwin/lib

OS_AFTER_LDFLAGS =\
			-lw \
			-lintl \
			-lsocket \
			-lnsl \
			-lgen \
			-lm \
			-ldl
endif

ifeq ($(OS_ARCH),AIX)

OS_BEFORE_LDFLAGS =\
			-L/usr/dt/lib
endif

ifeq ($(OS_ARCH),IRIX)
endif

ifeq ($(OS_ARCH),Linux)

OS_BEFORE_LDFLAGS = -L/usr/X11R6/lib 

endif

ifeq ($(OS_ARCH),HP-UX)

OS_BEFORE_LDFLAGS = -L$(DIST)/bin

endif

LDFLAGS		=\
			$(OS_BEFORE_LDFLAGS) \
			$(DIST_FLAGS) \
			$(XFE_FLAGS) \
			$(X_LD_FLAGS) \
			$(OS_AFTER_LDFLAGS)

##
## Test dependancies
##
#OTHER_DEPS = Makefile $(XFE_FLAGS)
OTHER_DEPS = $(XFE_FLAGS)

##
## Resource source rule
##
$(OBJDIR)/%.ad.c:: %.ad # Makefile
	@$(MAKE_OBJDIR)
	@echo 'char * fallback_resources[] = {' > $@; \
	./ad2c $< >> $@; \
	echo '0};' >> $@

##
## Resource object rule
##
$(OBJDIR)/%.ad.o: $(OBJDIR)/%.ad.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $<

##
## Binary link rule
##
$(OBJDIR)/%.exe: $(OBJDIR)/%.o $(OBJDIR)/%.ad.o $(OTHER_DEPS)
	@$(MAKE_OBJDIR)
	$(XFE_PURIFY) $(CC) -o $@ $< $(OBJDIR)/$*.ad.o $(LDFLAGS)
