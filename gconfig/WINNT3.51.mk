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
#

#
# Config stuff for WINNT 3.51
#
# This makefile defines the following variables:
# CPU_ARCH, OS_CFLAGS, and OS_DLLFLAGS.
# It has the following internal variables:
# OS_PROC_CFLAGS and OS_WIN_CFLAGS.

include $(GDEPTH)/gconfig/WIN32.mk

PROCESSOR := $(shell uname -p)
ifeq ($(PROCESSOR), I386)
	CPU_ARCH        = x386
	OS_PROC_CFLAGS += -D_X86_
else 
	ifeq ($(PROCESSOR), MIPS)
		CPU_ARCH        = MIPS
		OS_PROC_CFLAGS += -D_MIPS_
	else 
		ifeq ($(PROCESSOR), ALPHA)
			CPU_ARCH        = ALPHA
			OS_PROC_CFLAGS += -D_ALPHA_
		else 
			CPU_ARCH  = processor_is_undefined
		endif
	endif
endif

OS_WIN_CFLAGS += -W3
OS_CFLAGS     += -nologo $(OS_WIN_CFLAGS) $(OS_PROC_CFLAGS)
#OS_DLLFLAGS  += -nologo -DLL -PDB:NONE -SUBSYSTEM:WINDOWS
OS_DLLFLAGS   += -nologo -DLL -PDB:NONE -SUBSYSTEM:WINDOWS
#
# Win NT needs -GT so that fibers can work
#
OS_CFLAGS     += -GT
OS_CFLAGS     += -DWINNT
