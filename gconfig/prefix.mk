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
# Master "Core Components" for computing program prefixes             #
#######################################################################

#
# Object prefixes
#

ifndef OBJ_PREFIX
	OBJ_PREFIX = 
endif

#
# Library suffixes
#

ifndef LIB_PREFIX
	ifeq ($(OS_ARCH), WINNT)
		LIB_PREFIX = 
	else
		LIB_PREFIX = lib
	endif
endif


ifndef DLL_PREFIX
	ifeq ($(OS_ARCH), WINNT)
		DLL_PREFIX = 
	else
		DLL_PREFIX = lib
	endif
endif


ifndef IMPORT_LIB_PREFIX
	IMPORT_LIB_PREFIX = 
endif


ifndef PURE_LIB_PREFIX
	ifeq ($(OS_ARCH), WINNT)
		PURE_LIB_PREFIX =
	else
		PURE_LIB_PREFIX = purelib
	endif
endif

#
# Program prefixes
#

ifndef PROG_PREFIX
	PROG_PREFIX = 
endif

