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
ASFILES	=	x86LinuxException.s					\
		x86LinuxInvokeNative.s					\
		$(NULL)

CPPSRCS	+= 	x86LinuxThread.cpp					\
		$(NULL)

LOCAL_MD_EXPORTS_x86 +=  	x86LinuxThread.h			\
				$(NULL)

endif

ifeq ($(OS_ARCH),FreeBSD)
ASFILES	=	x86FreeBSDException.s					\
		x86FreeBSDInvokeNative.s					\
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
