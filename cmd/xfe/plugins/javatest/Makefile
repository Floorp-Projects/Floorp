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



DEPTH = ../../../..

# This is required for compilation of only this directory
ifeq ($(OS_ARCH),HP-UX)
CFLAGS += $(DSO_CFLAGS)
endif

CSRCS = npshell.c stubs.c
OBJS  = $(CSRCS:.c=.o)

LIBTARGETS = $(OBJDIR)/javatestplugin.so
TARGETS = targets

# Rules to build generated headers

JRI_HEADER_CLASSES =	       \
	JavaTestPlugin	       \
	java.lang.String       \
	java.awt.Window	       \
	java.awt.Component     \
	java.awt.Color	       \
	java.lang.Class

JRI_STUB_CLASSES =	       \
	JavaTestPlugin	       \
	java.lang.String       \
	java.awt.Window	       \
	java.awt.Component     \
	java.awt.Color

include $(DEPTH)/config/rules.mk

CLASSSRC := .:$(DEPTH)/sun-java/classsrc

INCLUDES += -I_gen

targets:
	@$(MAKE) classes
	@$(MAKE) jri_headers jri_stubs
	@$(MAKE) libtargets

JAVAC =	$(JAVAI) -classpath /usr/local/netscape/java/lib/javac.zip \
	     -ms8m sun.tools.javac.Main

classes: JavaTestPlugin.java
	$(JAVAC) -classpath $(CLASSSRC) JavaTestPlugin.java

libtargets: $(LIBTARGETS)

$(OBJDIR)/javatestplugin.so: $(OBJS)
	$(LD) $(DSO_LDOPTS) -o $@ $(OBJS) $(DSO_LDFLAGS)

install:: $(TARGETS)
	$(INSTALL) $(TARGETS) $(DIST)/bin

INCLUDES += -I$(DEPTH)/lib/plugin
GARBAGE += so_locations

clobber::
	rm -rf _gen _stubs so_locations

export:: targets
	$(INSTALL) $(LIBTARGETS) $(DIST)/lib

install:: export

$(OBJDIR)/stubs.o:		  \
	../common/npunix.c	  \
	_stubs/JavaTestPlugin.c	  \
	_stubs/java_lang_String.c \
	_stubs/java_awt_Color.c	  \
	_stubs/java_awt_EmbeddedFrame.c
