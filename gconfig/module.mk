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
# The master "Core Components" source and release component directory #
# names are ALWAYS identical and are the value of $(MODULE).          #
# NOTE:  A component is also called a module or a subsystem.          #
#######################################################################

#
#  All "Core Components" <component>-specific source-side tags must
#  always be identified for compiling/linking purposes
#

ifndef JAVA_SOURCE_COMPONENT
	JAVA_SOURCE_COMPONENT = java
endif

ifndef NETLIB_SOURCE_COMPONENT
	NETLIB_SOURCE_COMPONENT = netlib
endif

ifndef NSPR_SOURCE_COMPONENT
	NSPR_SOURCE_COMPONENT = nspr20
endif

ifndef SECTOOLS_SOURCE_COMPONENT
	SECTOOLS_SOURCE_COMPONENT = sectools
endif

ifndef SECURITY_SOURCE_COMPONENT
	SECURITY_SOURCE_COMPONENT = security
endif

