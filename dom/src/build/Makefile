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

DEPTH = ../../..

include $(DEPTH)/config/config.mk

LCFLAGS=-D_IMPL_NS_DOM

LIBRARY_NAME = jsdom

MODULE=raptor 

REQUIRES=xpcom raptor

CPPSRCS=nsDOMFactory.cpp nsHTMLTagsEnums.cpp

INCLUDES +=  -I$(PUBLIC)/xpcom -I$(PUBLIC)/raptor \
	-I$(PUBLIC)/dom -I$(PUBLIC)/js -I../base -I$(PUBLIC)/jsurl

ifeq ($(OS_ARCH),IRIX)
LD_ALL = -all
LD_NONE = -none
endif

#
# Rhapsody is using a more-portable way of specifying
# the link line, Linux and other OS's probably want
# to follow suit. -mcafee
#
ifeq ($(OS_TARGET),Rhapsody)
EXTRA_DSO_LDOPTS = \
	-L$(DIST)/lib \
	-ljsdombase_s	\
	-ljsdomcore_s	\
	-ljsdomevents_s	\
	-ljsdomhtml_s	\
	-ljsdomcss_s	\
	-ljsurl		\
	-L$(DIST)/bin	\
	-lraptorbase	\
	-lraptorgfx	\
	-lgfxunix	\
	-lgmbaseunix	\
	-lnetlib	\
	-ljs		\
	-lxpcom		\
	-lnspr21	\
	-lplds21	\
	-lplc21		\
	$(NULL)
else
# These must be maintained in the proper order for Linux.
EXTRA_DSO_LDOPTS = \
	$(LD_ALL)					\
	$(DIST)/lib/libjsdombase_s.a			\
	$(DIST)/lib/libjsdomcss_s.a			\
	$(DIST)/lib/libjsdomcore_s.a			\
	$(DIST)/lib/libjsdomevents_s.a			\
	$(DIST)/lib/libjsdomhtml_s.a			\
	$(DIST)/lib/libjsurl.a				\
	$(LD_NONE)					\
	$(DIST)/bin/libraptorbase.so			\
	$(DIST)/bin/libraptorgfx.so \
  $(DIST)/bin/libgfxunix.so \
	$(DIST)/bin/libgmbaseunix.so			\
	$(DIST)/bin/libnetlib.so			\
	$(DIST)/bin/libjs.so				\
	$(DIST)/bin/libxpcom.so				\
	$(DIST)/bin/libnspr21.so			\
	$(DIST)/bin/libplds21.so			\
	$(DIST)/bin/libplc21.so				\
	-lm
endif

include $(DEPTH)/config/rules.mk
