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

# Enable builds on any drive if defined.
!if !defined(MOZ_SRC)
MOZ_SRC=y:
!endif

# Enable builds from user defined top level directory.
!if !defined(MOZ_TOP)
MOZ_TOP=mozilla
!endif

#//------------------------------------------------------------------------
#// Defines specific to MOZ_NGLAYOUT
#//------------------------------------------------------------------------
!if defined(MOZ_NGLAYOUT)
NGLAYOUT_MAKEFILE=nglayout.mak
NGLAYOUT_ENV_VARS=STANDALONE_IMAGE_LIB=1 MODULAR_NETLIB=1 NGLAYOUT_BUILD_PREFIX=1
MOZNGLAYOUT_BRANCH=RAPTOR_INTEGRATION0_BRANCH
CVSCO = cvs -q co -P
!endif


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

NMAKE=@nmake -nologo -$(MAKEFLAGS)

#//------------------------------------------------------------------------
#//
#// Stuff a do complete pull and build
#//
#//------------------------------------------------------------------------

default:: 		build_all

pull_and_build_all::     pull_all    \
			    build_all

#// Do this when you pull a new tree, or else you will often get bugs
#// when replaceing an old dist with a new dist.

pull_clobber_build_all:: pull_all \
			clobber_all \
			build_all

clobber_build_all:: 	clobber_all \
			build_all

!if defined(MOZ_NGLAYOUT)
# The MOZ_NGLAYOUT pull is complicated, be very careful choosing which files are on 
# the tip and which are on the branches.
pull_all:: pull_client_source_product pull_nglayout pull_netlib repull_ngl_integration pull_imglib repull_include
!else
pull_all:: pull_client_source_product 
!endif

!if defined(MOZ_NGLAYOUT)
pull_nglayout:
	@cd $(MOZ_SRC)
	$(CVSCO) $(MOZ_TOP)/$(NGLAYOUT_MAKEFILE)
	@cd $(MOZ_SRC)/$(MOZ_TOP)
	$(NMAKE) -f $(NGLAYOUT_MAKEFILE) pull_nglayout $(NGLAYOUT_ENV_VARS)

pull_netlib:
	@cd $(MOZ_SRC)/$(MOZ_TOP)
	$(NMAKE) -f $(NGLAYOUT_MAKEFILE) pull_netlib $(NGLAYOUT_ENV_VARS)

# Here is where we pull everything on the layout integration branch
repull_ngl_integration:
	@cd $(MOZ_SRC)
	$(CVSCO) -r $(MOZNGLAYOUT_BRANCH) $(MOZ_TOP)/include $(MOZ_TOP)/cmd $(MOZ_TOP)/lib $(MOZ_TOP)/modules
	@cd $(MOZ_SRC)/$(MOZ_TOP)

# Careful to put this after repull_ngl_integration, want modules/libutil and 
# modules/libimg to be on imglib branch
pull_imglib:
	@cd $(MOZ_SRC)/$(MOZ_TOP)
	$(NMAKE) -f $(NGLAYOUT_MAKEFILE) pull_imglib $(NGLAYOUT_ENV_VARS)

# Want certain files in the include directory to be on the tip
repull_include:
	@cd $(MOZ_SRC)
	$(CVSCO) -A $(MOZ_TOP)/include/net.h
!endif


pull_client_source_product:
    @echo +++ client.mak: checking out the client with "$(CVS_BRANCH)"
    cd $(MOZ_SRC)\.
    -cvs -q co $(CVS_BRANCH)      MozillaSourceWin


!if defined(MOZ_NGLAYOUT)
# Build NGLayout first.
build_all:  build_nglayout \
			build_dist  \
			build_client
!else
build_all:  build_dist  \
			build_client
!endif

!if defined(MOZ_NGLAYOUT)
build_nglayout: 
	cd $(MOZ_SRC)\$(MOZ_TOP)
	$(NMAKE) -f $(NGLAYOUT_MAKEFILE) $(NGLAYOUT_ENV_VARS)
!endif

build_dist:
    @echo +++ client.mak: building dist
    cd $(MOZ_SRC)\$(MOZ_TOP)
    $(NMAKE) -f makefile.win


build_client:
    @echo +++ client.mak: building client
    cd $(MOZ_SRC)\$(MOZ_TOP)\cmd\winfe\mkfiles32
!if "$(MOZ_BITS)" == "16"
    $(NMAKE) -f mozilla.mak exports
!endif
    $(NMAKE) -f mozilla.mak DEPEND=1
    $(NMAKE) -f mozilla.mak


#
# remove all source files from the tree and print a report of what was missed
#
!if defined(MOZ_NGLAYOUT)
clobber_all:: clobber_moz clobber_nglayout
!else
clobber_all:: clobber_moz
!endif

clobber_moz:
    cd $(MOZ_SRC)\$(MOZ_TOP)
    $(NMAKE) -f makefile.win clobber_all
    cd $(MOZ_SRC)\$(MOZ_TOP)\cmd\winfe\mkfiles32
    $(NMAKE) -f mozilla.mak clobber_all
!if !defined(MOZ_MEDIUM)
    cd $(MOZ_SRC)\$(MOZ_TOP)\netsite\ldap\libraries\msdos\winsock
    $(NMAKE) -f nsldap.mak clobber_all
!endif

!if defined(MOZ_NGLAYOUT)
clobber_nglayout:
	cd $(MOZ_SRC)\$(MOZ_TOP)
	$(NMAKE) -f $(NGLAYOUT_MAKEFILE) clobber $(NGLAYOUT_ENV_VARS)
!endif

depend:
    -del /s /q make.dep
    $(NMAKE) -f makefile.win depend 

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

!if !EXIST( $(MOZ_SRC)\$(MOZ_TOP)\xyzzy.tmp )
ERR_MESSAGE=$(ERR_MESSAGE)^
MOZ_SRC isn't set correctly: [$(MOZ_SRC)\$(MOZ_TOP)]!=[$(MAKEDIR)]
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
		    [pull_dist]^
		    [pull_client]^
		    [build_all]^
		    [build_dist]^
		    [build_ldap]^
		    [build_client]^
^
Environment variables:^
^
MOZ_BITS    set to either 32 or 16 ^
MOZ_SRC     set to the directory above $(MOZ_TOP) or "$(MAKEDIR)\.."^
MOZ_TOOLS   set to the directory containing the java compiler see ^
		http://warp/tools/nt^
JAVA_HOME   set to the same thing as MOZ_TOOLS^
CVSROOT     set to the public mozilla cvs server^

!ERROR $(ERR_MESSAGE)

!endif

