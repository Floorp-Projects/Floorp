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
# Master "Core Components" macros to figure out binary code location  #
#######################################################################

#
# Figure out where the binary code lives.
#

BUILD         = $(PLATFORM)
OBJDIR        = $(PLATFORM)
DIST          = $(GDEPTH)/dist/$(PLATFORM)
DIST_LIB      = $(GDEPTH)/dist/$(PLATFORM)/lib
VPATH         = $(NSINSTALL_DIR)/$(PLATFORM)
DEPENDENCIES  = $(PLATFORM)/.md

# XXX - Need this for compatibility with 'old' config style
#       when your component requires something in the 'other' world
CONFIG_DIST          = $(GDEPTH)/dist/$(CONFIG_PLATFORM)
CONFIG_DIST_LIB      = $(GDEPTH)/dist/$(CONFIG_PLATFORM)/lib

ifdef BUILD_DEBUG_GC
	DEFINES += -DDEBUG_GC
endif

GARBAGE += $(DEPENDENCIES)
