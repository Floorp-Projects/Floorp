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

DEPTH=../../../..

include $(DEPTH)/config/config.mk

LIBRARY_NAME = raptorxmlcontent_s

DEFINES += -D_IMPL_NS_HTML

INCLUDES += \
	-I../../../html/base/src	\
	-I../../../html/document/src    \
	-I../../../html/content/src	\
	-I../../../html/style/src	\
	-I$(DIST)/public/raptor			\
	$(NULL)

# Note the sophisticated alphabetical ordering :-|
CPPSRCS =				\
	nsXMLElement.cpp		\
	$(NULL)

EXPORTS = \
	$(NULL)

MODULE = raptor

REQUIRES = xpcom raptor dom netlib js pref

MKSHLIB :=

include $(DEPTH)/config/rules.mk


export::

install:: $(TARGETS)


clobber::

