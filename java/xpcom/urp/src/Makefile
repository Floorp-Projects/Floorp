#!gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
# Igor Kushnirskiy <idk@eng.sun.com>
#
DEPTH           = ../../../../
topsrcdir	= $(DEPTH)
srcdir		= .
VPATH		= .

LIBRARY_NAME    = urpstubs
MODULE          = urpstubs
#IS_COMPONENT	= 1

include $(DEPTH)/config/autoconf.mk

include $(topsrcdir)/config/config.mk

DIRS = transport

CXXFLAGS        +=  -I$(CONNECT_SRC)/public -I$(topsrcdir)/java/xpcom/xpcom -Itransport
CPPSRCS =  \
     urpManager.cpp   \
     urpStub.cpp \
     $(NULL)
     
EXPORTS = \
       urpStub.h \
       urpManager.h \
       $(NULL)

LIBS     = \
        -lxpcom \
	-lxptinfo \
	$(NSPR_LIBS) \
	$(NULL)

include $(topsrcdir)/config/rules.mk





