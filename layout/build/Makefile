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

DEPTH = ../..

include $(DEPTH)/config/config.mk

LCFLAGS=-D_IMPL_NS_HTML 

LIBRARY_NAME = raptorhtml

MODULE=raptor 

REQUIRES=xpcom raptor 

CPPSRCS=dlldeps.cpp 

#INCLUDES +=  -I../html/base/src -I../base/src -I../css/layout/src

LD = $(CCC) -all

LOBJS = \
	$(DIST)/lib/libraptorlayout_s.a			\
	$(DIST)/lib/libraptorhtmlbase_s.a		\
	$(DIST)/lib/libnglcsslay_s.a			\
	$(DIST)/lib/libraptorhtmldoc_s.a		\
	$(DIST)/lib/libraptorhtmlforms_s.a		\
	$(DIST)/lib/libraptorhtmlstyle_s.a		\
	$(DIST)/lib/libraptorhtmltable_s.a		\
	$(DIST)/lib/libraptorevents_s.a


EXTRA_DSO_LDOPTS = \
	$(DIST)/bin/libraptorhtmlpars.so		\
	$(DIST)/bin/libraptorgfx.so			\
	$(DIST)/bin/libraptorbase.so			\
	$(DIST)/bin/libgmbaseunix.so			\
	$(DIST)/bin/libjsdom.so				\
	$(DIST)/bin/libnetlib.so			\
	$(DIST)/bin/libxpcom.so				\
	$(DIST)/bin/libnspr21.so			\
	$(DIST)/bin/libplds21.so			\
	$(DIST)/bin/libplc21.so				\
	-lm

include $(DEPTH)/config/rules.mk
