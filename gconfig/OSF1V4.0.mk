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

#
# On OSF1 V4.0, pthreads is the default implementation strategy.
# Classic nspr is also available.
#
ifneq ($(OS_RELEASE),V3.2)
	USE_PTHREADS = 1
	ifeq ($(CLASSIC_NSPR), 1)
		USE_PTHREADS =
		IMPL_STRATEGY := _CLASSIC
	endif
endif

#
# Config stuff for DEC OSF/1 V4.0
#
include $(GDEPTH)/gconfig/OSF1.mk

ifeq ($(OS_RELEASE),V4.0)
	OS_CFLAGS += -DOSF1V4
endif
