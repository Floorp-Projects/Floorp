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

