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

LIBRARY_NAME = nglhtmlcon_s

# Note the sophisticated alphabetical ordering :-|
CPPSRCS=				\
	nsHTMLAnchor.cpp		\
	nsHTMLApplet.cpp		\
	nsHTMLArea.cpp			\
	nsHTMLBR.cpp			\
	nsHTMLBase.cpp			\
	nsHTMLBaseFont.cpp		\
	nsHTMLBody.cpp			\
	nsHTMLComment.cpp		\
	nsHTMLDList.cpp			\
	nsHTMLDel.cpp			\
	nsHTMLDirectory.cpp		\
	nsHTMLDiv.cpp			\
	nsHTMLEmbed.cpp			\
	nsHTMLFont.cpp			\
	nsHTMLGenericContent.cpp	\
	nsHTMLHR.cpp			\
	nsHTMLHead.cpp			\
	nsHTMLHeading.cpp		\
	nsHTMLHtml.cpp			\
	nsHTMLInput.cpp			\
	nsHTMLIns.cpp			\
	nsHTMLLI.cpp			\
	nsHTMLLayer.cpp			\
	nsHTMLLink.cpp			\
	nsHTMLMap.cpp			\
	nsHTMLMenu.cpp			\
	nsHTMLMeta.cpp			\
	nsHTMLMod.cpp			\
	nsHTMLOList.cpp			\
	nsHTMLObject.cpp		\
	nsHTMLOption.cpp		\
	nsHTMLParagraph.cpp		\
	nsHTMLParam.cpp			\
	nsHTMLPre.cpp			\
	nsHTMLQuote.cpp			\
	nsHTMLScript.cpp		\
	nsHTMLStyle.cpp			\
	nsHTMLTable.cpp			\
	nsHTMLTableCaption.cpp		\
	nsHTMLTableCell.cpp		\
	nsHTMLTableCol.cpp		\
	nsHTMLTableRow.cpp		\
	nsHTMLTableSection.cpp		\
	nsHTMLTitle.cpp			\
	nsHTMLUList.cpp			\
	nsObjectContent.cpp		\
	$(NULL)

MODULE = raptor

REQUIRES = xpcom raptor dom js netlib

include $(DEPTH)/config/config.mk

LCFLAGS += -D_IMPL_NS_HTML

INCLUDES += -I../../style/src -I../../../base/src -I../../../css/layout/src \
 -I../../base/src

MKSLIB :=

include $(DEPTH)/config/rules.mk
