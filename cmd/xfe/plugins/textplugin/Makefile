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

include $(DEPTH)/config/config.mk

# This is required for compilation of only this directory
ifeq ($(OS_ARCH),HP-UX)
CFLAGS		+= $(DSO_CFLAGS)
DSO_LDOPTS	+= +e NP_GetValue +e NP_Initialize +e NP_Shutdown +e NP_GetMIMEDescription
endif

# On Linux ELF, motif cannot be used by plugin. So diable the motif part.
# and enable plugin trace
ifeq ($(OS_ARCH),Linux)
ifdef BUILD_UNIX_PLUGINS
CFLAGS		+= -DPLUGIN_TRACE -UTEXT_PLUGIN -I$(DEPTH)/dist/public/java
endif
else
CFLAGS		+= -DTEXT_PLUGIN -I$(DEPTH)/dist/public/java
endif

CSRCS		= npshell.c stubs.c
OBJS		= $(CSRCS:.c=.o)

ifeq ($(OS_ARCH),AIX)
LD		= svld
endif

#
# Defines to build the unix plugin sdk automatically
#
SDK_FILES	= README \
		  Test.html \
		  Test.txt \
		  makefile.hpux \
		  makefile.osf1 \
		  makefile.sgi \
		  makefile.sol23 \
		  makefile.sol24 \
		  makefile.sun4 \
		  makefile.linux \
		  npshell.c \
		  $(DEPTH)/cmd/xfe/plugins/common/npunix.c \
		  $(DEPTH)/lib/plugin/npupp.h \
		  $(DEPTH)/include/npapi.h \
		  $(DEPTH)/dist/public/java/jri.h \
		  $(DEPTH)/dist/public/java/jri_md.h

SDK_TEST_FILES	= $(wildcard $(DEPTH)/cmd/xfe/plugins/test/text*)

UNIX_SDK	= unix-sdk

TARGETS		= $(OBJDIR)/libtextplugin.so

all default:: $(TARGETS) install

install:: $(TARGETS)
	$(INSTALL) $(TARGETS) $(DIST)/bin

unix-sdk:: $(SDK_FILES)
	echo "Generating unix plugin sdk..."; \
	rm -rf $(OBJDIR)/sdk; \
	mkdir -p $(OBJDIR)/sdk; \
	cp $(SDK_FILES) $(OBJDIR)/sdk/.; \
	mkdir -p $(OBJDIR)/sdk/test; \
	cp $(SDK_TEST_FILES) $(OBJDIR)/sdk/test/.; \
	cd $(OBJDIR)/sdk; \
	FILES=`echo *`; \
	tar cvf - $$FILES | compress -c > $(UNIX_SDK).tar.Z; \
	rm -rf $$FILES

include $(DEPTH)/config/rules.mk

#DEFINES	+= -DPLUGIN_TRACE
INCLUDES	+= -I$(DEPTH)/lib/plugin
GARBAGE		+= so_locations $(UNIX_SDK)
OPTIMIZER	= -g

$(OBJDIR)/libtextplugin.so: $(OBJS)
	$(LD) $(DSO_LDOPTS) -o $@ $(OBJS) $(DSO_LDFLAGS) $(EXTRA_DSO_LDOPTS)

$(OBJDIR)/stubs.o: ../common/npunix.c
