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

DEPTH = ../..

LIBRARY_NAME = raptorhtmlpars

DEFINES = -D_IMPL_NS_HTMLPARS

CPPSRCS =     \
  nsHTMLContentSink.cpp \
  nsParserNode.cpp  \
  nsParserDebug.cpp \
  nsScanner.cpp   \
  nsToken.cpp   \
  nsTokenHandler.cpp \
  CNavDTD.cpp \
  COtherDTD.cpp \
  nsParser.cpp  \
  nsHTMLTokens.cpp  \
  prstrm.cpp \
  $(NULL)

EXPORTS =     \
  nshtmlpars.h    \
  nsIContentSink.h  \
  nsIHTMLContentSink.h  \
  nsHTMLTokens.h    \
  nsIParserNode.h   \
  nsIParser.h   \
  nsIParserDebug.h \
  nsIParserFilter.h \
  nsToken.h   \
  $(NULL)

MODULE = raptor

REQUIRES = xpcom netlib raptor

include $(DEPTH)/config/config.mk

include $(DEPTH)/config/rules.mk
