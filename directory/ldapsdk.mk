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

#
# Static macros.  Can be overridden on the command-line.
#
ifndef CVSROOT
CVSROOT		= /m/src
endif

# Allow for cvs flags
ifndef CVS_FLAGS
CVS_FLAGS = 
endif

CVS_CMD		= cvs $(CVS_FLAGS)
TARGETS		= export libs install
LDAP_MODULE      = DirectorySDKSource


#
# Environment variables.
#
ifdef MOZ_DATE
CVS_BRANCH      = -D "$(MOZ_DATE)"
endif

ifdef MOZ_BRANCH
CVS_BRANCH      = -r $(MOZ_BRANCH)
endif


ifndef MOZ_LDAPVER
MOZ_LDAPVER = -r DIRECTORY_C_SDK_30_BRANCH
endif


all: setup build

.PHONY: all setup build

setup:
ifdef LDAP_MODULE
	$(CVS_CMD) -q co $(MOZ_LDAPVER) $(LDAP_MODULE)
endif

build:
	cd config; $(MAKE)
	cd directory; $(MAKE) $(TARGETS)








