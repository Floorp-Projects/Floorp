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

LIBRARY_NAME = gfxgtk

MODULE=raptor 

REQUIRES=util img xpcom raptor netlib

LCFLAGS+=-D_IMPL_NS_GFXONXP -I/usr/lib/glib/include

LLIBS+=$(DIST)/lib/libxpcom.a $(DIST)/lib/libraptorbase.a $(DIST)/lib/libraptorgfx.a

CPPSRCS=nsDeviceContextGTK.cpp nsFontMetricsGTK.cpp nsGfxFactoryGTK.cpp \
	nsRenderingContextGTK.cpp nsImageGTK.cpp

CSRCS=gtklayout.c

include $(DEPTH)/config/rules.mk


