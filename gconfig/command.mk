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
# Master "Core Components" default command macros;                    #
# can be overridden in <arch>.mk                                      #
#######################################################################

AS            = $(CC)
ASFLAGS      += $(CFLAGS)
CCF           = $(CC) $(CFLAGS)
PURIFY        = purify $(PURIFYOPTIONS)
LINK_DLL      = $(LINK) $(OS_DLLFLAGS) $(DLLFLAGS)
LINK_EXE      = $(LINK) $(OS_LFLAGS) $(LFLAGS)
NFSPWD        = $(NSINSTALL_DIR)/nfspwd
CFLAGS       += $(OPTIMIZER) $(OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
		$(XCFLAGS)
RANLIB        = echo
JAR           = /bin/jar
TAR           = /bin/tar
#
# For purify
#
NOMD_CFLAGS  += $(OPTIMIZER) $(NOMD_OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
		$(XCFLAGS)

