# 
#           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
#              NETSCAPE COMMUNICATIONS CORPORATION
# Copyright (C) 1996 Netscape Communications Corporation.  All Rights
# Reserved.  Use of this Source Code is subject to the terms of the
# applicable license agreement from Netscape Communications Corporation.
# The copyright notice(s) in this Source Code does not indicate actual or
# intended publication of this Source Code.
#

ifeq ($(OS_ARCH),Linux)
ASFILES	=	x86LinuxException.s					\
		x86LinuxInvokeNative.s					\
		$(NULL)

CPPSRCS	+= 	x86LinuxThread.cpp					\
		$(NULL)

LOCAL_MD_EXPORTS_x86 +=  	x86LinuxThread.h			\
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



