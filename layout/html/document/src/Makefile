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

LIBRARY_NAME = raptorhtmldoc_s

DEFINES += -D_IMPL_NS_HTML

INCLUDES += \
	-I../../base/src		\
	-I../../forms/src		\
	-I../../style/src		\
	-I../../table/src		\
        -I../../dom		        \
	$(NULL)

# Note the sophisticated alphabetical ordering :-|
CPPSRCS =				\
	nsHTMLContentSink.cpp		\
	nsHTMLDocument.cpp		\
	nsHTMLIFrame.cpp    \
	nsHTMLParts.cpp    \
	$(NULL)

EXPORTS = \
	nsIHTMLDocument.h	\
	$(NULL)

INCLUDES += \
	-I../../base/src -I../../forms/src \
	-I../../style/src -I../../table/src \
	-I../../../base/src

MODULE = raptor

REQUIRES = xpcom raptor dom netlib js

MKSHLIB :=

include $(DEPTH)/config/rules.mk


export::

install:: $(TARGETS)
	$(INSTALL) ua.css $(DIST)/bin/res


clobber::
	rm -f $(DIST)/bin/res/ua.css

