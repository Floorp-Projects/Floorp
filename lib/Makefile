#! gmake
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

#  

DEPTH	= ..

include $(DEPTH)/config/config.mk

DIRS	= layout libcgi libi18n libmisc

ifndef NO_UNIX_LDAP
DIRS	+= ldap
endif

ifdef MOZ_MAIL_NEWS
DIRS	+= libmsg
endif

ifdef MOZ_NEO
DIRS	+= libneo
endif

DIRS	+= libnet libparse plugin xlate xp

ifdef MOZ_MAIL_NEWS
DIRS	+= libaddr libmime
endif

DIRS	+= htmldlgs

ifndef NO_LAYERS
DIRS	+= liblayer
endif

DIRS	+= libmocha libstyle

ifeq ($(OS_ARCH),OS2)
DIRS	+= libcnv
endif

DIRS	+= libpics

DIRS	+= mariner

DIRS	+= libpwcac

include $(DEPTH)/config/rules.mk


# This is a kludge so that running "make netscape-us" in this directory
#   will rebuild the subdirectories, and then relink ../../cmd/xfe/netscape-us
#
junk=export libs install
netscape-us netscape-export nav-us nav-export netscape-us.pure netscape-export.pure: $(junk)
	cd $(DEPTH)/cmd/xfe; $(MAKE) $@
