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

DEPTH = ../../../..

CPPSRCS = 	x86ExceptionHandler.cpp					\
		x86SysCallsRuntime.cpp					\
		$(NULL)

LOCAL_MD_EXPORTS_x86 =  x86SysCallsRuntime.h			\
			x86Win32ExceptionHandler.h		\
			$(NULL)

MODULE_NAME = EF

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

ifeq ($(OS_ARCH),Linux)
ASFILES	=	x86Exception_gas.s					\
		x86InvokeNative_gas.s					\
		$(NULL)

CPPSRCS	+= 	x86LinuxThread.cpp					\
		$(NULL)

LOCAL_MD_EXPORTS_x86 +=  	x86LinuxThread.h			\
				$(NULL)

endif

ifeq ($(OS_ARCH),FreeBSD)
ASFILES	=	x86Exception_gas.s					\
		x86InvokeNative_gas.s					\
		$(NULL)

CPPSRCS	+= 	x86FreeBSDThread.cpp					\
		$(NULL)

LOCAL_MD_EXPORTS_x86 +=  	x86FreeBSDThread.h			\
				$(NULL)

endif

ifeq ($(OS_ARCH),WINNT)
CPPSRCS	+= 	Win32ExceptionHandler.cpp                               \
		x86Win32InvokeNative.cpp				\
		x86Win32Thread.cpp					\
		$(NULL)

LOCAL_MD_EXPORTS_x86 +=  	x86Win32Thread.h			\
				$(NULL)
endif

include $(DEPTH)/config/rules.mk
