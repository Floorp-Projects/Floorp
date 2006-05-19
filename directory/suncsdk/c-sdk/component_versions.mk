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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
#

# NSPR - Netscape Portable Runtime
NSPR_LIBVERSION		= 4
NSPR_RELEASE_TAG	= v4.1.4

# NSS - Network Security Services
NSSVERS				= 3
NSS_RELEASE_TAG		= SECURITY_JES5_20060428

# SVRCORE - Client/server utility library
# SVRCORE_RELEASE_TAG	= SVRCORE_3_3_3_RTM

# LDAP library
LDAPVERS			= 60
LDAPVERS_SUFFIX		= 6.0

# PRLDAP library
PRLDAPVERS      	= 60
PRLDAPVERS_SUFFIX	= 6.0

# LBER library
LBERVERS			= 60
LBERVERS_SUFFIX		= 6.0

# ldif library
LDIFVERS    		= 60
LDIFVERS_SUFFIX		= 6.0

# iutil library
IUTILVERS   		= 60
IUTILVERS_SUFFIX	= 6.0

# util library
UTILVERS    		= 60
UTILVERS_SUFFIX 	= 6.0

# ssl library
SSLDAPVERS  		= 60
SSLDAPVERS_SUFFIX 	= 6.0

# sasl library
SASLVERS			= 20
SASL_RELEASE_TAG	= 2.19_20051117


# libICU - replaces National Language Support (NLS).
# ICU_LIBVERSION		= 2.1.6
# LIBICU_RELDATE		= 20040126_21.1

# Some components already had existing Solaris 5.8 symbolic
# link to a Solaris 5.6 version.  Hence, the new respun components
# were put in in a forte6 directory in each of the component
# respectively.  For Solaris 5.8 only we have to pick up the components
# from the forte6 directory.  As we move forward with new components,
# we can take the mess below out
# Michael.....
#ifeq ($(OS_ARCH), SunOS)
#    ifneq ($(USE_64), 1)
#	OS_VERS         := $(shell uname -r)
#	ifeq ($(OS_VERS),5.8)
#	    ifneq ($(OS_TEST),i86pc)
#		NSPR_RELEASE_TAG=v4.1.2/forte6
#		NSS_RELEASE_TAG =NSS_3_3_2_RTM/forte6
#		SVRCORE_RELEASE_TAG=SVRCORE_3_3_1_RTM/forte6
#	    endif
#	endif
#    endif
#endif

#ifeq ($(OS_ARCH), Linux)
#    ifeq ($(OS_RELEASE), 2.4)
#	NSPR_RELEASE_TAG=v4.1.2/redhat7.2
#	NSS_RELEASE_TAG =NSS_3_3_2_RTM/redhat7.2
#	SVRCORE_RELEASE_TAG=SVRCORE_3_3_1_RTM/redhat7.2
#    endif
#endif	
