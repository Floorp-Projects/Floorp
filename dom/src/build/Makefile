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

ifeq ($(USE_NGLAYOUT_GTK),1)
GFX_DSO_FLAGS  = -lgfxgtk
BASE_DSO_FLAGS = -lgmbasegtk
else
GFX_DSO_FLAGS  = -lgfxmotif
BASE_DSO_FLAGS = -lgmbasemotif
endif

EXTRA_DSO_LDOPTS = \
	$(LD_ALL) \
	-L$(DIST)/lib \
	-ljsdombase_s	\
	-ljsdomcore_s	\
	-ljsdomevents_s	\
	-ljsdomhtml_s	\
	-ljsdomcss_s	\
	-ljsurl		\
	$(LD_NONE) \
	-L$(DIST)/bin	\
	-lraptorbase	\
	-lraptorgfx	\
	$(GFX_DSO_FLAGS) \
	$(BASE_DSO_FLAGS) \
	-lnetlib	\
	-ljs		\
	-lxpcom		\
	-lnspr21	\
	-lplds21	\
	-lplc21		\
	$(NULL)

include $(DEPTH)/config/rules.mk
