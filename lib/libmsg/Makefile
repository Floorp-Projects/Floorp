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



DEPTH		= ../..

MODULE		= msg
LIBRARY_NAME	= mozmsg

CSRCS	= \
	ad_strm.c \
	ap_decod.c \
	ap_encod.c \
	appledbl.c \
	bh_strm.c \
	m_binhex.c \
	msgutils.c \
	$(NULL)


CPPSRCS	= \
	addrutil.cpp \
	biffmast.cpp \
	bytearr.cpp \
	chngntfy.cpp \
	dwordarr.cpp \
	filters.cpp \
	grec.cpp \
	grpinfo.cpp \
	hosttbl.cpp \
	idarray.cpp \
	imaphost.cpp \
	imapoff.cpp \
	jsmsg.cpp \
	listngst.cpp \
	maildb.cpp \
	mailhdr.cpp \
	mhtmlstm.cpp \
	msgbg.cpp \
	msgbgcln.cpp \
	msgccach.cpp \
	msgcflds.cpp \
	msgcmfld.cpp \
	msgcpane.cpp \
	msgdb.cpp \
	msgdbini.cpp \
	msgdbvw.cpp \
	msgdlqml.cpp \
	msgdwnof.cpp \
	msgfcach.cpp \
	msgfinfo.cpp \
	msgfpane.cpp \
	msgglue.cpp \
	msghdr.cpp \
	msgimap.cpp \
	msglpane.cpp \
	msglsrch.cpp \
	msgmapi.cpp \
	msgmast.cpp \
	msgmdn.cpp \
	msgmpane.cpp \
	msgmsrch.cpp \
	msgnsrch.cpp \
	msgoffnw.cpp \
	msgpane.cpp \
	msgppane.cpp \
	msgprefs.cpp \
	msgpurge.cpp \
	msgrulet.cpp \
	msgsend.cpp \
	msgsendp.cpp \
	msgspane.cpp \
	msgtpane.cpp \
	msgundac.cpp \
	msgundmg.cpp \
	msgurlq.cpp \
	msgzap.cpp \
	newsdb.cpp \
	newshdr.cpp \
	newshost.cpp \
	newspane.cpp \
	newsset.cpp \
	nwsartst.cpp \
	prsembst.cpp \
	ptrarray.cpp \
	search.cpp \
	subline.cpp \
	subpane.cpp \
	thrdbvw.cpp \
	thrhead.cpp \
	thrlstst.cpp \
	thrnewvw.cpp \
	$(NULL)

EXPORTS = \
	ad_codes.h \
	appledbl.h \
	bytearr.h \
	chngntfy.h \
	dberror.h \
	dwordarr.h \
	errcode.h \
	grec.h \
	grpinfo.h \
	hosttbl.h \
	idarray.h \
	imaphost.h \
	imapoff.h \
	jsmsg.h \
	listngst.h \
	m_binhex.h \
	m_cvstrm.h \
	maildb.h \
	mailhdr.h \
	mhtmlstm.h \
	msg.h \
	msg_opaq.h \
	msgbg.h \
	msgbgcln.h \
	msgbiff.h \
	msgccach.h \
	msgcflds.h \
	msgcmfld.h \
	msgcpane.h \
	msgdb.h \
	msgdbapi.h \
	msgdbtyp.h \
	msgdbvw.h \
	msgdlqml.h \
	msgdwnof.h \
	msgfcach.h \
	msgfinfo.h \
	msgfpane.h \
	msghdr.h \
	msghost.h \
	msgimap.h \
	msglpane.h \
	msgmast.h \
	msgmdn.h \
	msgmpane.h \
	msgoffnw.h \
	msgpane.h \
	msgppane.h \
	msgprefs.h \
	msgprnot.h \
	msgpurge.h \
	msgrulet.h \
	msgsec.h \
	msgsend.h \
	msgsendp.h \
	msgspane.h \
	msgstrob.h \
	msgtpane.h \
	msgundac.h \
	msgundmg.h \
	msgundo.h \
	msgurlq.h \
	msgutils.h \
	msgzap.h \
	neohdrs.h \
	newsdb.h \
	newshdr.h \
	newshost.h \
	newspane.h \
	newsrc.h \
	newsset.h \
	nwsartst.h \
	pmsgfilt.h \
	pmsgsrch.h \
	prsembst.h \
	ptrarray.h \
	subline.h \
	subpane.h \
	thrdbvw.h \
	threads.h \
	thrhead.h \
	thrlstst.h \
	thrnewvw.h \
	vwerror.h \


REQUIRES	= nspr dbm htmldlgs img util layer pref security js java net progress msg addr network


include $(DEPTH)/config/rules.mk

ifeq ($(subst /,_,$(shell uname -s)),OS2)
INCLUDES	+= -I$(PUBLIC)/neo
else
INCLUDES	+= $(G++INCLUDES)
endif

ifndef NO_UNIX_LDAP
INCLUDES	+= -I$(XPDIST)/public/ldap
DEFINES		+= -DLIBMSG_LDAP
endif

INCLUDES	+= -I$(DIST)/include -I$(PUBLIC)/security -I../libaddr -I$(PUBLIC)/msg
