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
IGNORE_MANIFEST=1
THIS_MAKEFILE=nglayout.mak
THAT_MAKEFILE=makefile.win

!if !defined(MOZ_TOP)
#enable builds from changed top level directories
MOZ_TOP=mozilla
!endif

#
# Command macro defines
#

!if defined(MOZ_DATE)
CVSCO = cvs -q co -P -D "$(MOZ_DATE)"
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


############################################################
## This should really be in a different file, like client.mak
## but it's OK here for now.

## Rules for pulling the source from the cvs repository
############################################################

pull_and_build_all: pull_seamonkey build_seamonkey

pull_all: pull_seamonkey

pull_nglayout: pull_lizard pull_xpcom pull_imglib pull_netlib pull_nglayout \
pull_editor

pull_seamonkey:
	cd $(MOZ_SRC)\.
	$(CVSCO) $(MOZ_TOP)/nsprpub
	$(CVSCO_LIZARD) SeaMonkeyEditor

pull_lizard:
	cd $(MOZ_SRC)\.
	$(CVSCO_LIZARD) $(MOZ_TOP)/LICENSE
	$(CVSCO_LIZARD) $(MOZ_TOP)/LEGAL
	$(CVSCO_LIZARD) $(MOZ_TOP)/config
	$(CVSCO_LIZARD) $(MOZ_TOP)/dbm
	$(CVSCO_LIZARD) $(MOZ_TOP)/lib/liblayer
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/zlib
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/libutil
	$(CVSCO_LIZARD) $(MOZ_TOP)/nsprpub
	$(CVSCO_LIZARD) $(MOZ_TOP)/sun-java
	$(CVSCO_LIZARD) $(MOZ_TOP)/nav-java
	$(CVSCO_LIZARD) $(MOZ_TOP)/js
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/security/freenav
	$(CVSCO_LIBPREF) $(MOZ_TOP)/modules/libpref
	$(CVSCO_PLUGIN) $(MOZ_TOP)/modules/plugin
	$(CVSCO_LIZARD) $(MOZ_TOP)/modules/oji
	$(CVSCO_LIZARD) $(MOZ_TOP)/caps
        $(CVSCO_LIZARD) $(MOZ_TOP)/rdf
        $(CVSCO_LIZARD) $(MOZ_TOP)/intl
!if defined(NGPREFS)
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/wincom
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/winfe/defaults.h
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/winfe/nsIDefaultBrowser.h
	$(CVSCO_LIZARD) $(MOZ_TOP)/cmd/winfe/prefs
!endif

pull_xpcom:
	@cd $(MOZ_SRC)\.
	$(CVSCO_XPCOM) $(MOZ_TOP)/modules/libreg 
	$(CVSCO_XPCOM) $(MOZ_TOP)/xpcom

pull_imglib:
	@cd $(MOZ_SRC)\.
	$(CVSCO_IMGLIB) $(MOZ_TOP)/jpeg
	$(CVSCO_IMGLIB) $(MOZ_TOP)/modules/libutil
	$(CVSCO_IMGLIB) $(MOZ_TOP)/modules/libimg 

pull_netlib:
	@cd $(MOZ_SRC)\.
	$(CVSCO_NETWORK) $(MOZ_TOP)/lib/xp
	$(CVSCO_NETWORK) $(MOZ_TOP)/lib/libpwcac
	$(CVSCO_NETWORK) $(MOZ_TOP)/network
	$(CVSCO_NETWORK) $(MOZ_TOP)/include

pull_nglayout:
	@cd $(MOZ_SRC)\.
	$(CVSCO_RAPTOR) $(MOZ_TOP)/base
	$(CVSCO_RAPTOR) $(MOZ_TOP)/dom
	$(CVSCO_RAPTOR) $(MOZ_TOP)/gfx
	$(CVSCO_RAPTOR) $(MOZ_TOP)/htmlparser
	$(CVSCO_RAPTOR) $(MOZ_TOP)/layout
	$(CVSCO_RAPTOR) $(MOZ_TOP)/view
	$(CVSCO_RAPTOR) $(MOZ_TOP)/webshell
	$(CVSCO_RAPTOR) $(MOZ_TOP)/widget
	$(CVSCO_RAPTOR) $(MOZ_TOP)/xpfe

pull_editor:
	@cd $(MOZ_SRC)\.
	$(CVSCO_RAPTOR) $(MOZ_TOP)/editor

############################################################

clobber_all:: clobber_nglayout

clobber_nglayout:
	@cd $(MOZ_SRC)\mozilla\.
	nmake -f nglayout.mak clobber_all $(NGLAYOUT_ENV_VARS)

build_all: build_seamonkey build_apprunner

build_seamonkey:
	@cd $(MOZ_SRC)\mozilla\.
	nmake -f nglayout.mak all

build_apprunner:
	@cd $(MOZ_SRC)\mozilla\xpfe\.
	nmake -f makefile.win
