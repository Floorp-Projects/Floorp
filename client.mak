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

DEPTH=.

!if !defined(MOZ_TOP)
#enable builds from changed top level directories
MOZ_TOP=mozilla
!endif

#
# Command macro defines
#

#//------------------------------------------------------------------------
#// Figure out how to do the pull.
#//------------------------------------------------------------------------
# uncomment these, modify branch tag, and check in to branch for milestones
#MOZ_BRANCH=SeaMonkey_M17_BRANCH
#NSPR_CO_TAG=SeaMonkey_M17_BRANCH
#PSM_CO_TAG=SeaMonkey_M17_BRANCH
#LDAP_SDK_CO_TAG=SeaMonkey_M17_BRANCH

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

!if "$(MOZ_CVS_FLAGS)" != ""
CVS_FLAGS=$(MOZ_CVS_FLAGS)
!endif

# let's be explicit about CVSROOT... some windows cvs clients
# are too stupid to correctly work without the -d option 
#
#  if they are too stupid, they should fail.  I am
#  commenting this out because this does not work
#  under 4nt.  (%'s are evaluted differently)
#
#  If it breaks you, mail dougt@netscape.com
#  and leaf@mozilla.org
#
!if 0
!if defined(CVSROOT)
CVS_FLAGS=$(CVS_FLAGS) -d "$(CVSROOT)"
!endif
!endif

CVSCO = cvs -q $(CVS_FLAGS) co $(CVS_BRANCH) -P

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

#//------------------------------------------------------------------------
#// Figure out how to pull NSPR.
#// If no NSPR_CO_TAG is specified, use the default static tag
#//------------------------------------------------------------------------


!if "$(NSPR_CO_TAG)" != ""
NSPR_CO_FLAGS=-r $(NSPR_CO_TAG)
!else
NSPR_CO_FLAGS=-r NSPRPUB_CLIENT_BRANCH
!endif

CVSCO_NSPR = cvs -q $(CVS_FLAGS) co $(NSPR_CO_FLAGS) -P

#//------------------------------------------------------------------------
#// Figure out how to pull PSM client libs.
#// If no PSM_CO_TAG is specified, use the default static tag
#//------------------------------------------------------------------------

!if "$(PSM_CO_TAG)" != ""
PSM_CO_FLAGS=-r $(PSM_CO_TAG)
!else
PSM_CO_FLAGS=-r SECURITY_CLIENT_BRANCH
!endif

CVSCO_PSM = cvs -q $(CVS_FLAGS) co $(PSM_CO_FLAGS) -P

#//------------------------------------------------------------------------
#// Figure out how to pull LDAP C SDK client libs.
#// If no LDAPCSDK_CO_TAG is specified, use the default tag
#//------------------------------------------------------------------------

!if "$(LDAPCSDK_CO_TAG)" != ""
LDAPCSDK_CO_FLAGS=-r $(LDAPSDK_CO_TAG)
!else
LDAPCSDK_CO_FLAGS=-r LDAPCSDK_40_BRANCH
!endif

CVSCO_LDAPCSDK = cvs -q $(CVS_FLAGS) co $(LDAPCSDK_CO_FLAGS) -P

## The master target
############################################################

pull_and_build_all: pull_all depend build_all


## Rules for pulling the source from the cvs repository
############################################################

pull_clobber_and_build_all: pull_all clobber_all build_all

pull_all: pull_nspr pull_psm pull_ldapcsdk pull_seamonkey

pull_nspr: pull_clientmak
      cd $(MOZ_SRC)\.
      $(CVSCO_NSPR) mozilla/nsprpub

pull_psm:
	cd $(MOZ_SRC)\.
	$(CVSCO_PSM) mozilla/security

pull_ldapcsdk:
	cd $(MOZ_SRC)\.
	$(CVSCO_LDAPCSDK) mozilla/directory/c-sdk

pull_xpconnect:
	cd $(MOZ_SRC)\.
	$(CVSCO_NSPR) mozilla/nsprpub
	$(CVSCO) mozilla/include
	$(CVSCO) mozilla/config
	$(CVSCO) -l mozilla/js
	$(CVSCO) -l mozilla/js/src
	$(CVSCO) mozilla/js/src/fdlibm
	$(CVSCO) mozilla/js/src/xpconnect
	$(CVSCO) mozilla/modules/libreg
	$(CVSCO) mozilla/xpcom

# pull either layout only or seamonkey the browser
pull_layout:
	cd $(MOZ_SRC)\.
	$(CVSCO) RaptorWin

pull_seamonkey: pull_clientmak
	cd $(MOZ_SRC)\.
	$(CVSCO) SeaMonkeyAll

pull_clientmak:
    cd $(MOZ_SRC)\.
    $(CVSCO) mozilla/client.mak

############################################################

# nmake has to be hardcoded, or we have to depend on mozilla/config
# being pulled already to figure out what $(NMAKE) should be.

clobber_all: clobber_nspr clobber_psm clobber_seamonkey

build_all: build_nspr build_seamonkey

clobber_nspr:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\nsprpub
	nmake -f makefile.win clobber_all

clobber_psm:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\security
	nmake -f makefile.win clobber_all

clobber_xpconnect:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	-rd /s /q dist
	set DIST_DIRS=1
	@cd $(MOZ_SRC)\$(MOZ_TOP)\nsprpub
	nmake -f makefile.win clobber_all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\include
	nmake -f makefile.win clobber_all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\modules\libreg
	nmake -f makefile.win clobber_all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\xpcom
	nmake -f makefile.win clobber_all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\js
	nmake -f makefile.win clobber_all

clobber_seamonkey:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	-rd /s /q dist
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

build_nspr:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\nsprpub
	nmake -f makefile.win export

build_psm:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\security
	nmake -f makefile.win export

build_xpconnect:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\nsprpub
	nmake -f makefile.win all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\include
	nmake -f makefile.win all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\modules\libreg
	nmake -f makefile.win all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\xpcom
	nmake -f makefile.win all
	@cd $(MOZ_SRC)\$(MOZ_TOP)\js\src
	nmake -f makefile.win all

build_seamonkey:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake -f makefile.win all

build_client:
	@cd $(MOZ_SRC)\mozilla\.
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

install:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake -f makefile.win install

export:
	@cd $(MOZ_SRC)\$(MOZ_TOP)\nsprpub
	nmake -f makefile.win export
	@cd $(MOZ_SRC)\$(MOZ_TOP)\security
	nmake -f makefile.win export
	@cd $(MOZ_SRC)\$(MOZ_TOP)\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake -f makefile.win export

clobber_dist:
	@cd $(MOZ_SRC)\mozilla\.
	set DIST_DIRS=1
	nmake -f makefile.win clobber_all

clobber_client:
	@cd $(MOZ_SRC)\mozilla\.
	set CLIENT_DIRS=1
	nmake -f makefile.win clobber_all

clobber_layout:
	@cd $(MOZ_SRC)\mozilla\.
	set LAYOUT_DIRS=1
	nmake -f makefile.win clobber_all

browse_info::
	cd $(MOZ_SRC)\$(MOZ_TOP)
	-dir /s /b *.sbr > sbrlist.tmp
	-bscmake /Es /o mozilla.bsc @sbrlist.tmp
	-rm sbrlist.tmp

regchrome::
	@cd $(MOZ_SRC)\mozilla\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake /f makefile.win regchrome

deliver::
	@cd $(MOZ_SRC)\mozilla\.
	set DIST_DIRS=1
	set LAYOUT_DIRS=1
	set CLIENT_DIRS=1
	nmake /f makefile.win splitsymbols

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
