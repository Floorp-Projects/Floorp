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

