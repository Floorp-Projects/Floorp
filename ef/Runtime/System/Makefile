#!gmake
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
# (1) Directory specific info                                         #
#######################################################################

DEPTH = ../..

DIRS = md
SUBMODULES = $(DIRS)

CPPSRCS = 	ClassWorld.cpp						\
		FieldOrMethod.cpp					\
		JavaObject.cpp						\
		JavaString.cpp						\
		Method.cpp						\
		StackWalker.cpp						\
		InterfaceDispatchTable.cpp				\
		SysCallsRuntime.cpp					\
		JavaVM.cpp						\
		Monitor.cpp						\
		Thread.cpp						\
		$(NULL)

LOCAL_EXPORTS = ClassWorld.h						\
		InterfaceDispatchTable.h				\
		JavaObject.h						\
		FieldOrMethod.h						\
		Exceptions.h						\
		JavaString.h						\
		Method.h						\
		Monitor.h						\
		StackWalker.h						\
		SysCallsRuntime.h					\
		Thread.h						\
		JavaVM.h						\
		Monitor.h						\
		$(NULL)

MODULE_NAME = EF

#######################################################################
# (2) Include "component" configuration information.                  #
#######################################################################

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################


#######################################################################
# (4) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(DEPTH)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################



