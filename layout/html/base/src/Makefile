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
CPPSRCS=				\
	nsAbsoluteFrame.cpp		\
	nsBRFrame.cpp			\
	nsBlockFrame.cpp		\
	nsBodyFrame.cpp			\
	nsContainerFrame.cpp		\
	nsFrame.cpp			\
	nsFrameReflowState.cpp		\
	nsGlobalVariables.cpp		\
	nsHRFrame.cpp			\
	nsHTMLAtoms.cpp			\
	nsHTMLContainerFrame.cpp	\
	nsHTMLIIDs.cpp			\
	nsHTMLFrame.cpp			\
	nsHTMLReflowCommand.cpp		\
	nsImageFrame.cpp		\
	nsImageMap.cpp			\
	nsInlineFrame.cpp		\
	nsInlineReflow.cpp		\
	nsLeafFrame.cpp			\
	nsLineLayout.cpp		\
	nsObjectFrame.cpp		\
	nsPageFrame.cpp			\
	nsPlaceholderFrame.cpp		\
	nsPresShell.cpp			\
	nsSimplePageSequence.cpp \
	nsScrollFrame.cpp		\
	nsSpacerFrame.cpp		\
	nsSplittableFrame.cpp		\
	nsTextFrame.cpp			\
	nsTextTransformer.cpp		\
	nsWBRFrame.cpp			\
	$(NULL)

EXPORTS =				\
	nsGlobalVariables.h		\
	nsIHTMLContent.h	\
	$(NULL)

MODULE = raptor

REQUIRES = xpcom raptor dom netlib plugin java js oji pref

include $(DEPTH)/config/config.mk

LCFLAGS += -D_IMPL_NS_HTML

INCLUDES += -I../../style/src -I../../content/src -I../../../base/src -I.  -I$(DIST)/oji

MKSHLIB :=

include $(DEPTH)/config/rules.mk

install::
	$(INSTALL) broken-image.gif $(DIST)/bin/res/html
