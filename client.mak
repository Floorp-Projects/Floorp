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

#//------------------------------------------------------------------------
#// Figure out how to do the pull.
#//------------------------------------------------------------------------
!if "$(MOZ_BRANCH)" != ""
CVS_BRANCH=-r $(MOZ_BRANCH)
HAVE_BRANCH=1
!else
HAVE_BRANCH=0
!endif

!if "$(MOZ_DATE)" != ""
CVS_BRANCH=-D "$(MOZ_DATE)"
HAVE_DATE=1
!else
HAVE_DATE=0
!endif

!if $(HAVE_DATE) && $(HAVE_BRANCH)
ERR_MESSAGE=$(ERR_MESSAGE)^
Cannot specify both MOZ_BRANCH and MOZ_DATE
!endif

# let's be explicit about CVSROOT... some windows cvs clients
# are too stupid to correctly work without the -d option 
!if defined(CVSROOT)
CVSCO = cvs -d $(CVSROOT) -q co $(CVS_BRANCH) -P
!else
CVSCO = cvs -q co $(CVS_BRANCH) -P
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
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake -f makefile.win clobber_all 

depend:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake -f makefile.win depend 

build_all:
	@cd $(MOZ_SRC)\mozilla\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake -f makefile.win all

build_layout:
	@cd $(MOZ_SRC)\mozilla\.
	set LAYOUT_DIRS=1
	nmake -f makefile.win all

build_dist:
	@cd $(MOZ_SRC)\mozilla\.
	set DIST_DIRS=1
	nmake -f makefile.win all

clobber_dist:
	@cd $(MOZ_SRC)\mozilla\.
	rd /s /q dist
	set DIST_DIRS=1
	nmake -f makefile.win clobber_all

clobber_layout:
	@cd $(MOZ_SRC)\mozilla\.
	rd /s /q dist
	set LAYOUT_DIRS=1
	nmake -f makefile.win clobber_all

browse_info::
	cd $(MOZ_SRC)\$(MOZ_TOP)
	-dir /s /b *.sbr > sbrlist.tmp
	-bscmake /n /o nglayout.bsc @sbrlist.tmp
	-rm sbrlist.tmp

#//------------------------------------------------------------------------
#// Utility stuff...
#//------------------------------------------------------------------------

#//------------------------------------------------------------------------
# Verify that MOZ_SRC is set correctly
#//------------------------------------------------------------------------

# Check to see if it is set at all
!if "$(MOZ_SRC)"!=""

#
# create a temp file at the root and make sure it is visible from MOZ_SRC
#
!if [copy $(MAKEDIR)\client.mak $(MAKEDIR)\xyzzy.tmp > NUL] == 0
!endif

!if !EXIST( $(MOZ_SRC)\mozilla\xyzzy.tmp )
ERR_MESSAGE=$(ERR_MESSAGE)^
MOZ_SRC isn't set correctly: [$(MOZ_SRC)\mozilla]!=[$(MAKEDIR)]
!endif

!if [del $(MAKEDIR)\xyzzy.tmp]
!endif

!else
# MOZ_SRC isn't set at all
ERR_MESSAGE=$(ERR_MESSAGE)^
Environment variable MOZ_SRC isn't set.
!endif

#//------------------------------------------------------------------------
# Verify that MOZ_BITS is set
#//------------------------------------------------------------------------
!if !defined(MOZ_BITS)
ERR_MESSAGE=$(ERR_MESSAGE)^
Environment variable MOZ_BITS isn't set.
!endif

!if !defined(MOZ_TOOLS)
ERR_MESSAGE=$(ERR_MESSAGE)^
Environment variable MOZ_TOOLS isn't set.
!endif


#//------------------------------------------------------------------------
#// Display error 
#//------------------------------------------------------------------------

!if "$(ERR_MESSAGE)" != ""
ERR_MESSAGE = ^
client.mak:  ^
$(ERR_MESSAGE) ^
^
client.mak: usage^
^
nmake -f client.mak [MOZ_BRANCH=<cvs_branch_name>] ^
		    [MOZ_DATE=<cvs_date>]^
		    [pull_and_build_all]^
		    [pull_all]^
		    [build_all]^
^
Environment variables:^
^
MOZ_BITS    set to 32^
MOZ_SRC     set to the directory above mozilla or "$(MAKEDIR)\.."^
MOZ_TOOLS   set to the directory containing the needed tools ^

!ERROR $(ERR_MESSAGE)

!endif
