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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

#######################################################################
# Master "Core Components" default command macros;                    #
# can be overridden in <arch>.mk                                      #
#######################################################################

AS            = $(CC)
ASFLAGS      += $(CFLAGS)
CCF           = $(CC) $(CFLAGS)
PURIFY        = purify $(PURIFYOPTIONS)
LINK_DLL      = $(LINK) $(OS_DLLFLAGS) $(DLLFLAGS)
LINK_EXE      = $(LINK) $(OS_LFLAGS) $(LFLAGS)
CFLAGS       += $(OPTIMIZER) $(OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
		$(XCFLAGS)
RANLIB        = echo
TAR           = /bin/tar
#
# For purify
#
NOMD_CFLAGS  += $(OPTIMIZER) $(NOMD_OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
		$(XCFLAGS)

