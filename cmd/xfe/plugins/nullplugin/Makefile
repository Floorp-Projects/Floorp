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



DEPTH		= ../../../..

CSRCS		= npshell.c stubs.c nullplugin.c

PLUGIN		= $(OBJDIR)/libnullplugin.so
TARGETS		= $(PLUGIN)

include $(DEPTH)/config/rules.mk

# This is required for compilation of only this directory
ifeq ($(OS_ARCH),HP-UX)
CFLAGS		+= $(DSO_CFLAGS)
endif

ifeq ($(OS_ARCH),AIX)
LD		= svld
endif

ifeq ($(OS_ARCH)$(OS_RELEASE)$(CPU_ARCH),Linux2.0x86)
DYNMOTIF	= -L/usr/X11R6/lib -lXm
STATMOTIF	= /usr/X11R6/lib/libXm.a
TARGETS		+= $(OBJDIR)/libnullplugin-dynMotif.so
endif

#DEFINES	+= -DPLUGIN_TRACE
INCLUDES	+= -I$(DEPTH)/lib/plugin 

ifdef MOZ_JAVA
INCLUDES	+= -I$(DEPTH)/sun-java/include
else
INCLUDES	+= -I$(DEPTH)/sun-java/stubs
endif

install:: $(TARGETS)
	$(INSTALL) $(TARGETS) $(DIST)/bin

$(OBJDIR)/libnullplugin-dynMotif.so: $(OBJS)
	$(LD) $(DSO_LDOPTS) -o $@ $(OBJS) $(DSO_LDFLAGS) $(EXTRA_DSO_LDOPTS) $(DYNMOTIF)

$(PLUGIN): $(OBJS)
	$(LD) $(DSO_LDOPTS) -o $@ $(OBJS) $(DSO_LDFLAGS) $(EXTRA_DSO_LDOPTS) $(STATMOTIF)

$(OBJDIR)/stubs.c: ../common/npunix.c
