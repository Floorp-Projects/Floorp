# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

COMPONENTS_DIR		= /share/builds/components

# NSPR - Netscape Portable Runtime
NSPR_LIBVERSION		= 4
NSPR_RELEASE_TAG	= v4.2.2
NSPR_RELEASE_TREE	= $(COMPONENTS_DIR)

# NSS - Network Security Services
NSSVERS			= 3
NSS_RELEASE_TAG		= NSS_3_7_7_RTM
NSS_DYNAMIC_SOFTOKN	= 1
NSS_RELEASE_TREE	= $(COMPONENTS_DIR)

# SVRCORE - Client/server utility library
SVRCORE_RELEASE_TAG	= SVRCORE_3_3_RTM
SVRCORE_RELEASE_TREE	= $(COMPONENTS_DIR)

# LDAP library
LDAPVERS		= 50
LDAPVERS_SUFFIX		= 5.0

# PRLDAP library
PRLDAPVERS      	= 50
PRLDAPVERS_SUFFIX	= 5.0

# LBER library
LBERVERS		= 50
LBERVERS_SUFFIX		= 5.0

# ldif library
LDIFVERS    		= 50
LDIFVERS_SUFFIX		= 5.0

# iutil library
IUTILVERS   		= 50
IUTILVERS_SUFFIX	= 5.0

# util library
UTILVERS    		= 50
UTILVERS_SUFFIX 	= 5.0

# ssl library
SSLDAPVERS  		= 50
SSLDAPVERS_SUFFIX 	= 5.0


# libNLS - National Language Support.
NLS_LIBVERSION		= 31
LIBNLS_RELDATE		= v3.2
LIBNLS_RELEASE_TREE	= $(COMPONENTS_DIR)

# Some components already had existing Solaris 5.8 symbolic
# link to a Solaris 5.6 version.  Hence, the new respun components
# were put in in a forte6 directory in each of the component
# respectively.  For Solaris 5.8 only we have to pick up the components
# from the forte6 directory.  As we move forward with new components,
# we can take the mess below out
# Michael.....
ifeq ($(OS_ARCH), SunOS)
    ifneq ($(USE_64), 1)
	OS_VERS         := $(shell uname -r)
	ifeq ($(OS_VERS),5.8)
	    ifneq ($(OS_TEST),i86pc)
		NSPR_RELEASE_TAG=v4.1.2/forte6
		NSS_RELEASE_TAG =NSS_3_3_1_RTM/forte6
		SVRCORE_RELEASE_TAG=SVRCORE_3_3_RTM/forte6
		LIBNLS_RELDATE=v3.2/forte6
	    endif
	endif
    endif
endif
