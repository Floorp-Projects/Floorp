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



DEPTH		= ../..

MODULE		= mailto
LIBRARY_NAME	= mailto

CSRCS	= \
	ad_strm.c \
	ap_decod.c \
	ap_encod.c \
	appledbl.c \
	bh_strm.c \
	m_binhex.c \
	mprmime.c \
	msgutils.c \
	$(NULL)


CPPSRCS	= \
	addrutil.cpp \
	bytearr.cpp \
	dwordarr.cpp \
	memstrem.cpp \
	mhtmlstm.cpp \
	mprdecod.cpp \
	msgbg.cpp \
	msgcflds.cpp \
	msgcpane.cpp \
	msgglue.cpp \
	msgmast.cpp \
	msgmdn.cpp \
	msgpane.cpp \
	msgppane.cpp \
	msgprefs.cpp \
	msgsend.cpp \
	msgsendp.cpp \
	msgurlq.cpp \
	msgzap.cpp \
	ptrarray.cpp \
	$(NULL)
# We'd like to include msgdlqml.cpp eventually (deliver queued mail)


EXPORTS = \
	ad_codes.h \
	appledbl.h \
	bytearr.h \
	dwordarr.h \
	errcode.h \
	error.h \
	m_binhex.h \
	mhtmlstm.h \
	mprdecod.h \
	msg.h \
	msgcflds.h \
	msgcpane.h \
	msghdr.h \
	msgmast.h \
	msgmdn.h \
	msgpane.h \
	msgppane.h \
	msgprefs.h \
	msgprnot.h \
	msgsend.h \
	msgsendp.h \
	msgutils.h \
	msgzap.h \
	ptrarray.h \
	$(NULL)

REQUIRES	= nspr htmldlgs img util layer pref security js java net progress network


include $(DEPTH)/config/rules.mk

# INCLUDES	+= -I$(DIST)/include -I$(PUBLIC)/security -I../libaddr -I$(PUBLIC)/msg
