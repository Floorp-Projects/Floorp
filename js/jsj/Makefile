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

ifdef MOZ_JAVA
DIRS		= classes
endif

MODULE		= java
LIBRARY_NAME	= jsj

REQUIRES	= java js applet nspr img util layer

CSRCS		= \
		  jsjava.c \
		  jsStubs.c \
		  $(NULL)


JRI_GEN		= netscape.javascript.JSObject netscape.javascript.JSException

ifdef MOZ_JAVA
CSRCS	 += jsjsa.c      \
	    stubs.c      \
	    jsj_nodl.c    \
	    $(NULL)
EXPORTS		= jsjava.h \
		  $(JRI_GEN_DIR)/netscape_javascript_JSObject.h \
		  $(JRI_GEN_DIR)/netscape_javascript_JSException.h \
		  $(NULL)

else
EXPORTS = jsjava.h	 \
          $(NULL)
endif


include $(DEPTH)/config/rules.mk

######################################################################

# Generate jsj_nodl.c (so that you can check it in)
# These make rules only works on IRIX...sigh

ifeq ($(OS_ARCH),IRIX)
jsj_nodl.c: $(OBJDIR)/stubs.o Makefile $(DEPTH)/config/nodl.pl
	rm -f $@
	$(PERL) $(DEPTH)/config/nodl.pl "jsj_nodl_tab" \
	    `nm -Bn $(OBJDIR)/stubs.o | egrep Java_.*_stub | awk '{print $$3;}'` > $@
endif

ifdef MOZ_JAVA
$(OBJDIR)/stubs.o:					 \
	$(JRI_GEN_DIR)/netscape_javascript_JSObject.c	 \
	$(JRI_GEN_DIR)/netscape_javascript_JSException.c \
	$(NULL)
else
$(OBJDIR)/stubs.o:
endif

export:: install
