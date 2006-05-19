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

!if !defined(MOZ_TOP)
#enable builds from changed top level directories
MOZ_TOP=mozilla
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


LDAPSDK_BRANCH =-r LDAPSDK_40_BRANCH

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

# In theory, we should use some symbol in ns/config/liteness.mak,
# but we haven't pulled the file yet.  So, refer to MOZ_LITE and
# MOZ_MEDIUM explicitly .
pull_all:: 


#    -cvs co $(LDAPSDK_BRANCH) DirectorySDKSource



#pull_client_source_product:
#x-CEB-x    @echo +++ client.mak: checking out the client with "$(CVS_BRANCH)"
#x-CEB-x    cd $(MOZ_SRC)\.
#x-CEB-x    -cvs -d ":pserver:$(USERNAME)@cvsserver:/m/pub" co $(CVS_BRANCH) MozillaSourceWin
#x-CEB-x    -cvs -d ":pserver:$(USERNAME)@cvsserver:/m/pub" co $(CVS_BRANCH) mozilla/lib/libnet

build_all:              build_ldap  



build_ldap:
    @echo +++ ldapsdk.mak: building ldap
    cd $(MOZ_SRC)\mozilla\directory\c-sdk\ldap\libraries\msdos\winsock
    @echo +++ ldapsdk.mak: depend step
    $(NMAKE) -f nsldap.mak DEPEND=1
    @echo +++ ldapsdk.mak: build step
     $(NMAKE) -f nsldap.mak
    @echo +++ ldapsdk.mak: library creation
    $(NMAKE) -f nsldap.mak static 
    $(NMAKE) -f nsldap.mak dynamic
    $(NMAKE) -f nsldap.mak EXPORT=1

#
# remove all source files from the tree and print a report of what was missed
#
clobber_all:
    cd $(MOZ_SRC)\mozilla\directory\c-sdk\ldap\libraries\msdos\winsock
    $(NMAKE) -f nsldap.mak clobber_all

depend:
    -del /s /q make.dep
    $(NMAKE) -f makefile.win depend

quick:
    @cd $(MOZ_SRC)\.
    @cvs co ns/quickup
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
!if [copy $(MAKEDIR)\ldapsdk.mak $(MAKEDIR)\xyzzy.tmp > NUL] == 0
!endif

!if !EXIST( $(MOZ_SRC)\mozilla\directory\xyzzy.tmp )
ERR_MESSAGE=$(ERR_MESSAGE)^
MOZ_SRC isn't set correctly: [$(MOZ_SRC)\mozilla\directory]!=[$(MAKEDIR)]
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
ldapsdk.mak:  ^
$(ERR_MESSAGE) ^
^
ldapsdk.mak: usage^
^
nmake -f ldapsdk.mak [MOZ_BRANCH=<cvs_branch_name>] ^
		    [MOZ_DATE=<cvs_date>]^
		    [pull_and_build_all]^
		    [pull_all]^
		    [build_all]^
		    [build_ldap]^

^
Environment variables:^
^
MOZ_BITS    set to either 32 or 16 ^
MOZ_SRC     set to the directory above ns or "$(MAKEDIR)\.."^
MOZ_TOOLS   set to the directory containing the java compiler see ^
		http://warp/tools/nt^
JAVA_HOME   set to the same thing as MOZ_TOOLS^

!ERROR $(ERR_MESSAGE)

!endif

