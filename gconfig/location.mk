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
# Master "Core Components" macros to figure out binary code location  #
#######################################################################

#
# Figure out where the binary code lives.
#

BUILD         = $(PLATFORM)
OBJDIR        = $(PLATFORM)
DIST          = $(GDEPTH)/dist/$(PLATFORM)
VPATH         = $(NSINSTALL_DIR)/$(PLATFORM)
DEPENDENCIES  = $(PLATFORM)/.md

# XXX - Need this for compatibility with 'old' config style
#       when your component requires something in the 'other' world
CONFIG_DIST          = $(GDEPTH)/dist/$(CONFIG_PLATFORM)

ifdef BUILD_DEBUG_GC
	DEFINES += -DDEBUG_GC
endif

GARBAGE += $(DEPENDENCIES) core $(wildcard core.[0-9]*)
