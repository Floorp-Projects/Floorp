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

LIBRARY_NAME = raptorhtmlbase_s

# Note the sophisticated alphabetical ordering :-|
CPPSRCS =				\
	nsAbsoluteFrame.cpp		\
	nsBRPart.cpp			\
	nsBlockFrame.cpp		\
	nsBodyFrame.cpp			\
	nsBodyPart.cpp			\
	nsGlobalVariables.cpp		\
	nsHRPart.cpp			\
	nsHTMLAtoms.cpp			\
	nsHTMLBase.cpp			\
	nsHTMLBullet.cpp		\
	nsHTMLContainer.cpp		\
	nsHTMLContainerFrame.cpp	\
	nsHTMLContent.cpp		\
	nsHTMLHead.cpp			\
	nsHTMLIIDs.cpp			\
	nsHTMLImage.cpp			\
	nsHTMLReflowCommand.cpp \
	nsHTMLTagContent.cpp		\
	nsHTMLTags.cpp			\
	nsImageMap.cpp			\
	nsInlineFrame.cpp		\
	nsLeafFrame.cpp			\
	nsPageFrame.cpp			\
	nsPlaceholderFrame.cpp		\
	nsRootPart.cpp			\
	nsScrollFrame.cpp		\
	nsSpacerPart.cpp		\
	nsTextContent.cpp		\
	nsWBRPart.cpp			\
	nsDOMIterator.cpp		\
	nsDOMAttributes.cpp		\
	nsLineLayout.cpp		\
	$(NULL)
EXPORTS =				\
	nsGlobalVariables.h		\
	$(NULL)

MODULE = raptor

REQUIRES = xpcom raptor dom netlib js

include $(DEPTH)/config/config.mk

LCFLAGS += -D_IMPL_NS_HTML

INCLUDES += -I../../style/src -I../../../base/src

include $(DEPTH)/config/rules.mk

install::
	$(INSTALL) broken-image.gif $(DIST)/bin/res/html
