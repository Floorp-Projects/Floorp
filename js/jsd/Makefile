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

MODULE		= jsdebug
LIBRARY_NAME	= jsd

# Switching on MOZ_JAVA is a short-term hack. This is really for no Java/IFC
ifdef MOZ_JAVA
JAVA_OR_OJI = 1
endif
ifdef MOZ_OJI
JAVA_OR_OJI = 1
endif

ifdef JAVA_OR_OJI
DIRS		= classes
endif

REQUIRES	= java js nspr 

EXPORTS		= jsdebug.h

CSRCS		= \
		  jsdebug.c \
		  jsd_high.c \
		  jsd_hook.c \
		  jsd_scpt.c \
		  jsd_stak.c \
		  jsd_text.c \
		  jsd_lock.c \
		  $(NULL)

ifndef MOZ_JSD
CSRCS		+= jsdstubs.c jsd_java.c

JDK_GEN		= \
		  netscape.jsdebug.Script \
		  netscape.jsdebug.DebugController \
		  netscape.jsdebug.JSThreadState \
		  netscape.jsdebug.JSStackFrameInfo \
		  netscape.jsdebug.JSPC \
		  netscape.jsdebug.JSSourceTextProvider \
		  netscape.jsdebug.JSErrorReporter \
		  $(NULL)
endif

include $(DEPTH)/config/rules.mk

$(OBJDIR)\jsdstubs.o: \
	$(JDK_STUB_DIR)\netscape_jsdebug_Script.c \
	$(JDK_STUB_DIR)\netscape_jsdebug_DebugController.c \
	$(JDK_STUB_DIR)\netscape_jsdebug_JSThreadState.c \
	$(JDK_STUB_DIR)\netscape_jsdebug_JSStackFrameInfo.c \
	$(JDK_STUB_DIR)\netscape_jsdebug_JSPC.c \
	$(JDK_STUB_DIR)\netscape_jsdebug_JSSourceTextProvider.c	
