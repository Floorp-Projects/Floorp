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

ifeq ($(OS_ARCH),SunOS)
	CPU_ARCH = $(shell uname -p)
endif


#
# Force the IRIX64 machines to use IRIX.
#
ifeq ($(OS_ARCH),IRIX)
	CPU_ARCH = mips
endif


ifeq ($(OS_ARCH),AIX)
	CPU_ARCH = ppc
endif


#
# OS_OBJTYPE is used only by Linux
#

ifeq ($(OS_ARCH),Linux)
	OS_OBJTYPE := ELF
	CPU_ARCH := $(shell uname -m)
	ifneq (,$(findstring 86,$(CPU_ARCH)))
		CPU_ARCH = x86
	endif
endif

ifeq ($(OS_ARCH),HP-UX)
	CPU_ARCH = hppa
endif

ifeq ($(OS_ARCH),WINNT)
	CPU_ARCH = $(shell uname -p)
	ifeq ($(CPU_ARCH),I386)
		CPU_ARCH = x86
	endif
endif



