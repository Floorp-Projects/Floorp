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
# Master "Core Components" suffixes                                   #
#######################################################################

#
# Object suffixes
#

ifndef OBJ_SUFFIX
	ifeq ($(OS_ARCH), WINNT)
		OBJ_SUFFIX = .obj
	else
		OBJ_SUFFIX = .o
	endif
endif

#
# Resource suffixes
#

ifndef RC_SUFFIX
	ifeq ($(OS_ARCH), WINNT)
		RC_SUFFIX = .rc
		RES_SUFFIX = .res
	else
		RC_SUFFIX =
		RES_SUFFIX =
	endif
endif

#
# Library suffixes
#

STATIC_LIB_EXTENSION =

ifndef DYNAMIC_LIB_EXTENSION
	ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
		DYNAMIC_LIB_EXTENSION = _shr
	else
		DYNAMIC_LIB_EXTENSION =
	endif
endif


ifndef STATIC_LIB_SUFFIX
	STATIC_LIB_SUFFIX = .$(LIB_SUFFIX)
endif


ifndef DYNAMIC_LIB_SUFFIX
	DYNAMIC_LIB_SUFFIX = .$(DLL_SUFFIX)
endif


ifndef IMPORT_LIB_SUFFIX
	ifeq ($(OS_ARCH), WINNT)
		IMPORT_LIB_SUFFIX = .$(LIB_SUFFIX)
	else
		IMPORT_LIB_SUFFIX = 
	endif
endif


ifndef PURE_LIB_SUFFIX
	ifeq ($(OS_ARCH), WINNT)
		PURE_LIB_SUFFIX =
	else
		ifdef DSO_BACKEND
			PURE_LIB_SUFFIX = .$(DLL_SUFFIX)
		else
			PURE_LIB_SUFFIX = .$(LIB_SUFFIX)
		endif
	endif
endif


ifndef STATIC_LIB_SUFFIX_FOR_LINKING
	STATIC_LIB_SUFFIX_FOR_LINKING = $(STATIC_LIB_SUFFIX)
endif


ifndef DYNAMIC_LIB_SUFFIX_FOR_LINKING
	ifeq ($(OS_ARCH), WINNT)
		DYNAMIC_LIB_SUFFIX_FOR_LINKING = $(IMPORT_LIB_SUFFIX)
	else
		DYNAMIC_LIB_SUFFIX_FOR_LINKING = $(DYNAMIC_LIB_SUFFIX)
	endif
endif

#
# Program suffixes
#

ifndef PROG_SUFFIX
	ifeq ($(OS_ARCH), WINNT)
		PROG_SUFFIX = .exe
	else
		PROG_SUFFIX =
	endif
endif
