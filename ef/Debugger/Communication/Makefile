#!gmake
#
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
# (1) Directory specific info                                         #
#######################################################################

DEPTH = ../..

LIBRARY_NAME = DebuggerChannel

CPPSRCS =			DebuggerChannel.cpp 		\
					$(NULL)

EXPORTS =			DebuggerChannel.h 	\
					$(NULL)

LIBRARIES = libnspr21	\
			$(NULL)

#######################################################################
# (2) Include "component" configuration information.                  #
#######################################################################

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

ifeq ($(OS_ARCH),WINNT)
LDFLAGS += /ENTRY:DllEntryPoint kernel32.lib
endif

#######################################################################
# (4) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(DEPTH)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

libs:: $(SHARED_LIBRARY)
