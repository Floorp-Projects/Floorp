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

DEPTH=../../..

LIBRARY_NAME = raptorevents_s

DEFINES += -D_IMPL_NS_HTML

CPPSRCS =				\
	nsEventListenerManager.cpp	\
	nsEventStateManager.cpp	\
	nsDOMEvent.cpp			\
	$(NULL)

MODULE = raptor

EXPORTS = \
	nsEventListenerManager.h \
	nsEventStateManager.h \
	$(NULL)

REQUIRES = xpcom raptor dom js

include $(DEPTH)/config/config.mk

INCLUDES += -I$(PUBLIC)/dom

MKSHLIB :=

include $(DEPTH)/config/rules.mk

