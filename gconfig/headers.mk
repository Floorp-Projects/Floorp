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
# Master "Core Components" include switch for support header files    #
#######################################################################

#
#  Always append source-side machine-dependent (md) and cross-platform
#  (xp) include paths
#

INCLUDES  += -I$(SOURCE_MDHEADERS_DIR) 

ifneq ($(OS_TARGET),WIN16)
INCLUDES  += -I$(SOURCE_XPHEADERS_DIR)
endif

#
#  Only append source-side private cross-platform include paths for
#  sectools
#

ifeq ($(MODULE), sectools)
	INCLUDES += -I$(SOURCE_XPPRIVATE_DIR)
endif
