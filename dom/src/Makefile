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

DEPTH=../..

LIBRARY_NAME = jsdom

DEFINES = -D_IMPL_NS_DOM

CPPSRCS =				\
	nsJSEnvironment.cpp		\
	nsJSNode.cpp		\
	nsJSDocument.cpp	\
	nsJSDocumentFragment.cpp\
	nsJSElement.cpp		\
	nsJSText.cpp		\
	nsJSNodeIterator.cpp\
	nsJSAttribute.cpp	\
	nsJSAttributeList.cpp\
	nsJSWindow.cpp\
	nsGlobalWindow.cpp \
	$(NULL)

MODULE = raptor

REQUIRES = xpcom raptor dom js

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk
