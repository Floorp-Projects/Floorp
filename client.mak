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

!if !defined(MOZ_SRC)
#enable builds on any drive if defined.
MOZ_SRC=y:
!endif

!if !defined(MOZ_TOP)
#enable builds from changed top level directories
MOZ_TOP=ns
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

#//
#// Temporary hardcode (while we figure out how to do this)
#//	to support the Core modularity efforts...
#//

!ifndef MOZ_JAVAVER
!ifdef MOZ_MEDIUM
MOZ_JAVAVER    =-r JAVA_STUB_RELEASE_19980319
!else
MOZ_JAVAVER    =-r JAVA_RELEASE_19980304
!endif
!endif


NSPR20_BRANCH  =-r NSPR20_RELEASE_19980304_BRANCH
SECURITY_BRANCH=-r SECURITY_RELEASE_19980210
CORECONF_BRANCH=-r CFG_1_6
DBM_BRANCH     =-r DBM_RELEASE_19980319

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

# In theory, we should use some symbol in $(MOZ_TOP)/config/liteness.mak, 
# but we haven't pulled the file yet.  So, refer to MOZ_LITE and
# MOZ_MEDIUM explicitly .
!if defined(MOZ_LITE) || defined(MOZ_MEDIUM)
pull_all:: pull_client_source_product
!else
pull_all:: pull_client 
!endif

!ifndef NO_SECURITY
pull_security:
    -cvs -q co $(CORECONF_BRANCH) $(MOZ_TOP)/coreconf
    -cvs -q co $(SECURITY_BRANCH) CoreSecurity
!else
pull_security:
!endif

pull_client: pull_security
    @echo +++ client.mak: checking out the client with "$(CVS_BRANCH)"
    cd $(MOZ_SRC)\.
    -cvs -q co $(DBM_BRANCH)      $(MOZ_TOP)/dbm
    -cvs -q co $(CVS_BRANCH)      Client50Win
    -cvs -q co $(MOZ_JAVAVER)     JavaWin
    -cvs -q co $(NSPR20_BRANCH)   CoreNSPR20

pull_client_source_product:
    @echo +++ client.mak: checking out the client with "$(CVS_BRANCH)"
    cd $(MOZ_SRC)\.
    -cvs -q co $(DBM_BRANCH)      $(MOZ_TOP)/dbm
    -cvs -q co $(CVS_BRANCH)      MozillaSourceWin
    -cvs -q co $(MOZ_JAVAVER)     JavaStubWin


build_all:              build_ldap  \
			build_dist  \
			build_client
build_dist:
    @echo +++ client.mak: building dist
    cd $(MOZ_SRC)\$(MOZ_TOP)
    $(NMAKE) -f makefile.win


!if defined(MOZ_LITE) || defined(MOZ_MEDIUM)
build_ldap:
!else
build_ldap:
    @echo +++ client.mak: building ldap
    cd $(MOZ_SRC)\$(MOZ_TOP)\netsite\ldap\libraries\msdos\winsock
    $(NMAKE) -f nsldap.mak DEPEND=1
    $(NMAKE) -f nsldap.mak 
    $(NMAKE) -f nsldap.mak EXPORT=1
!endif


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
clobber_all:
    cd $(MOZ_SRC)\$(MOZ_TOP)
    $(NMAKE) -f makefile.win clobber_all
    cd $(MOZ_SRC)\$(MOZ_TOP)\cmd\winfe\mkfiles32
    $(NMAKE) -f mozilla.mak clobber_all
!if !defined(MOZ_MEDIUM)
    cd $(MOZ_SRC)\$(MOZ_TOP)\netsite\ldap\libraries\msdos\winsock
    $(NMAKE) -f nsldap.mak clobber_all
!endif

depend:
    -del /s /q make.dep
    $(NMAKE) -f makefile.win depend 

quick:
    @cd $(MOZ_SRC)\.
    @cvs -q co $(MOZ_TOP)/quickup
    @cd $(MOZ_SRC)\$(MOZ_TOP)\quickup
    @$(MOZ_TOOLS)\perl5\perl doupdate.pl

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

!ERROR $(ERR_MESSAGE)

!endif

