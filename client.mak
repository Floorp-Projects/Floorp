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

DEPTH=.

!if !defined(MOZ_TOP)
#enable builds from changed top level directories
MOZ_TOP=mozilla
!endif

#
# Arbitrary required defines (should probably just turn them on in the
#                              tree by default)

MODULAR_NETLIB = 1
STANDALONE_IMAGE_LIB = 1
NGLAYOUT_PLUGINS = 1

#
# Command macro defines
#

# let's be explicit about CVSROOT... some windows cvs clients
# are too stupid to correctly work without the -d option 
!if defined(MOZ_DATE)
CVSCO = cvs -q co -P -D "$(MOZ_DATE)"
!elseif defined(CVSROOT)
CVSCO = cvs -d $(CVSROOT) -q co -P
!else
CVSCO = cvs -q co -P
!endif

CVSCO_TAG = cvs -q co -P

# Branch tags we use

IMGLIB_BRANCH =
PLUGIN_BRANCH =
XPCOM_BRANCH =

!if defined(MOZ_DATE)
# CVS commands to pull the appropriate branch versions
CVSCO_LIBPREF = $(CVSCO)
CVSCO_PLUGIN = $(CVSCO)
!else
# CVS commands to pull the appropriate branch versions
CVSCO_LIBPREF = $(CVSCO) -A
CVSCO_PLUGIN = $(CVSCO) -A
!endif

CVSCO_XPCOM = $(CVSCO)
CVSCO_IMGLIB = $(CVSCO)
CVSCO_RAPTOR = $(CVSCO)
CVSCO_LIZARD = $(CVSCO)
CVSCO_NETWORK = $(CVSCO)

## The master target
############################################################

pull_and_build_all: pull_all build_all


## Rules for pulling the source from the cvs repository
############################################################

pull_and_build_all: pull_all build_all

pull_all: pull_seamonkey

# pull either layout only or seamonkey the browser
pull_layout:
	cd $(MOZ_SRC)\.
	$(CVSCO) RaptorWin

pull_seamonkey:
	cd $(MOZ_SRC)\.
	$(CVSCO) SeaMonkeyAll

############################################################

# nmake has to be hardcoded, or we have to depend on mozilla/config
# being pulled already to figure out what $(NMAKE) should be.

clobber_all:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	rd /s /q dist
	nmake -f makefile.win clobber_all 

depend:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	nmake -f makefile.win depend 

build_all:
	@cd $(MOZ_SRC)\mozilla\.
	set BUILD_CLIENT=1
	nmake -f makefile.win all

build_layout:
	@cd $(MOZ_SRC)\mozilla\.
	nmake -f makefile.win all
