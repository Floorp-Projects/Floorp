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

DEPTH        = ../..

MODULE		= addr
LIBRARY_NAME	= addr

CSRCS		= line64.c vobject.c vcc.c abfault.c abdeque.c abcolumn.c

CPPSRCS		= ab.cpp \
		  export.cpp \
		  import.cpp \
		  abcinfo.cpp \
		  abpane2.cpp \
		  abcpane.cpp \
		  abglue.cpp \
		  abntfy.cpp \
                  namecomp.cpp \
                  abpicker.cpp \
                  addrprsr.cpp \
                  abview.cpp \
                  abtracer.cpp \
                  abthumb.cpp \
                  abtable.cpp \
                  abstring.cpp \
                  abstore.cpp \
                  absearch.cpp \
                  abrow.cpp \
                  abprint.cpp \
                  abpart.cpp \
                  abobject.cpp \
                  abobjset.cpp \
                  abnamset.cpp \
                  abmodel.cpp \
                  abfile.cpp \
                  abenv.cpp \
                  abdefalt.cpp \
                  abdebug.cpp \
                  abcolset.cpp \
                  abchange.cpp \
                  abcell.cpp \
		  $(NULL)

EXPORTS         = aberror.h \
                  ab.h \
                  abcallbk.h \
                  abcinfo.h \
                  abcpane.h \
                  abinfo.h \
                  abntfy.h \
                  abpane2.h \
                  ldif.h \
                  abmodel.h \
                  namecomp.h \
                  abpicker.h \
                  addrprsr.h \
                  abtable.h \
                  abdeque.h \
                  $(NULL)

REQUIRES	= net msg dbm nspr img util layer pref js ldap security

include $(DEPTH)/config/rules.mk

ifndef NO_UNIX_LDAP
INCLUDES	+= -I$(DIST)/include -I$(PUBLIC)/security -I../libmsg -I$(PUBLIC)/ldap \
		-I$(PUBLIC)/neo -I$(PUBLIC)/msg
DEFINES		+= -DLIBMSG_LDAP
endif

ifeq ($(OS_ARCH),OS2)
OS_CFLAGS	+= -Fi- -Si-
endif

DEFINES		+= -DqNeoStandAlone
















