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

LIBRARY_NAME = gfxunix

MODULE=raptor 

REQUIRES=util img xpcom raptor netlib

LCFLAGS+=-D_IMPL_NS_GFXONXP 

EXTRA_DSO_LDOPTS+=\
	$(DIST)/bin/libxpcom.so \
	$(DIST)/bin/libraptorgfx.so \
	$(DIST)/bin/libraptorbase.so \
	$(DIST)/bin/libreg.so \
	$(LIBNSPR)\
	$(DIST)/lib/libplc21.a -lXt -lX11

CPPSRCS=nsDeviceContextUnix.cpp nsFontMetricsUnix.cpp nsGfxFactoryUnix.cpp \
	nsRenderingContextUnix.cpp nsRegionUnix.cpp nsImageUnix.cpp

include $(DEPTH)/config/rules.mk


