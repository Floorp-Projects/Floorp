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
# This make file was copied from ns/cmd/xfe/XfeWidgets/tests/Makefile
# and tweaked.  A lot of the rules and logic can probably be shared.
# Stuff should be shared in
#
#
DEPTH			= ../../../..

ifdef XFE_WIDGETS_BUILD_UNUSED
UNUSED_CSRCS		= \
				prog1.c \
				prog2.c \
				prog3.c \
				tree1.c \
				tree2.c \
				tree3.c \
				tree4.c \
				tree5.c \
				$(NULL)
endif

CSRCS			= \
				$(UNUSED_CSRCS) \
				demo.c \
				folder1.c \
				folder2.c \
				folder3.c \
				folder4.c \
				grid1.c \
				grid2.c \
				grid3.c \
				grid4.c \
				grid5.c \
				grid6.c \
				uil1.c \
				util1.c \
				$(NULL)

REQUIRES		= \
				Microline

PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=.exe))

include $(DEPTH)/config/rules.mk
include $(DEPTH)/cmd/xfe/XfeWidgets/XfeWidgets.mk

all:: $(PROGS)
install:: $(PROGS)

LDFLAGS =

SHARED_XFE_LIBS = $(DIST)/bin/libXmL.$(DLL_SUFFIX)

STATIC_XFE_LIBS = $(DIST)/lib/libXmL.a

DIST_FLAGS = -L$(DIST)/bin

ifeq ($(OS_ARCH),AIX)

  XFE_FLAGS	= $(STATIC_XFE_LIBS)

else

  XFE_FLAGS	= $(SHARED_XFE_LIBS)

endif

ifeq ($(OS_ARCH),Linux)
-include $(DEPTH)/config/motif.mk
endif

XM_LD_FLAGS = -lMrm -lXm $(NS_MOTIF2_XP_LD_FLAGS)

X_LD_FLAGS = $(XM_LD_FLAGS) -lXt -lXmu -lXext -lX11

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
#$(OBJDIR)/%.ad.c:: %.ad # Makefile
#	@$(MAKE_OBJDIR)
#	@echo 'char * fallback_resources[] = {' > $@; \
#	./ad2c $< >> $@; \
#	echo '0};' >> $@

##
## Resource object rule
##
#$(OBJDIR)/%.ad.o: $(OBJDIR)/%.ad.c
#	@$(MAKE_OBJDIR)
#	$(CC) -o $@ -c $<

##
## Binary link rule
##
$(OBJDIR)/%.exe: $(OBJDIR)/%.o $(OTHER_DEPS)
	@$(MAKE_OBJDIR)
	$(XFE_PURIFY) $(CC) -o $@ $< $(LDFLAGS)
