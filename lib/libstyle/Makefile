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

MODULE		= style
LIBRARY_NAME	= style

CSRCS		= csslex.c \
		  csstab.c \
		  csstojs.c \
		  jssrules.c \
		  libstyle.c \
		  stystack.c \
		  stystruc.c \
		  $(NULL)

EXPORTS		= stystack.h stystruc.h css.h

REQUIRES	= js nspr dbm img util layer

include $(DEPTH)/config/rules.mk

TEST_LIBS	= $(LIBRARY) \
		  $(DIST)/lib/liburl.$(LIB_SUFFIX) \
		  $(DIST)/lib/libsec-us.$(LIB_SUFFIX) \
		  $(DIST)/lib/libdbm.$(LIB_SUFFIX) \
		  $(DIST)/lib/libxp.$(LIB_SUFFIX) \
		  $(DIST)/lib/nspr.$(LIB_SUFFIX)

CSS_TEST_LIBS	= $(LIBRARY) \
		  $(DIST)/lib/libxp.$(LIB_SUFFIX)

ifeq ($(OS_ARCH),OS2)
$(OBJDIR)/csstab.o: csstab.c
	@$(MAKE_OBJDIR)
	$(CC) -Fo$@ -Sa -c $(CFLAGS) $<
endif

test:
	$(CC) -g -o $(OBJDIR)/style_test $(CFLAGS) -DSS_TEST stystruc.c $(TEST_LIBS)
	$(CC) -g -o $(OBJDIR)/stack_test $(CFLAGS) -DTEST_STYLESTACK stystack.c $(TEST_LIBS)

css_test:
	rm -f $(OBJDIR)/css_test
	$(CC) -g -o $(OBJDIR)/css_test $(CFLAGS) -DTEST_CSS_TRANSLATION csstojs.c $(CSS_TEST_LIBS)
