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

#######################################################################
# Master "Core Components" file system "release" prefixes             #
#######################################################################

#	RELEASE_TREE = $(GDEPTH)/../coredist


ifndef RELEASE_TREE
	RELEASE_TREE = /m/dist

	ifeq ($(OS_TARGET), WINNT)
		RELEASE_TREE = \\helium\dist
	endif

	ifeq ($(OS_TARGET), WIN95)
		RELEASE_TREE = \\helium\dist
	endif

	ifeq ($(OS_TARGET), WIN16)
		RELEASE_TREE = \\helium\dist
	endif
endif

RELEASE_XP_DIR = 
RELEASE_MD_DIR = $(PLATFORM)


REPORTER_TREE = $(subst \,\\,$(RELEASE_TREE))

