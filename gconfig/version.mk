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
# Build master "Core Components" release version directory name       #
#######################################################################

#
# Always set CURRENT_VERSION_SYMLINK to the <current> symbolic link.
#

CURRENT_VERSION_SYMLINK = current


#
#  For the sake of backwards compatibility (*sigh*) ...
#

ifndef VERSION
	ifdef BUILD_NUM
		VERSION = $(BUILD_NUM)
	endif
endif

ifndef RELEASE_VERSION
	ifdef BUILD_NUM
		RELEASE_VERSION = $(BUILD_NUM)
	endif
endif

#
# If VERSION has still NOT been set on the command line,
# as an environment variable, by the individual Makefile, or
# by the <component>-specific "version.mk" file, set VERSION equal
# to $(CURRENT_VERSION_SYMLINK).

ifndef VERSION
	VERSION = $(CURRENT_VERSION_SYMLINK)
endif

# If RELEASE_VERSION has still NOT been set on the command line,
# as an environment variable, by the individual Makefile, or
# by the <component>-specific "version.mk" file, automatically
# generate the next available version number via a perl script.
# 

ifndef RELEASE_VERSION
	RELEASE_VERSION = 
endif

#
# Set <component>-specific versions for compiliation and linkage.
#

ifndef JAVA_VERSION
	JAVA_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef NETLIB_VERSION
	NETLIB_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef NSPR_VERSION
	NSPR_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef SECTOOLS_VERSION
	SECTOOLS_VERSION = $(CURRENT_VERSION_SYMLINK)
endif

ifndef SECURITY_VERSION
	SECURITY_VERSION = $(CURRENT_VERSION_SYMLINK)
endif
