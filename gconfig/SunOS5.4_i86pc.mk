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
# Config stuff for Solaris 2.4 on x86
#

include $(GDEPTH)/gconfig/UNIX.mk

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC		= gcc
	OS_CFLAGS	+= -Wall -Wno-format
	CCC		= g++
	CCC		+= -Wall -Wno-format
	ASFLAGS		+= -x assembler-with-cpp
	ifdef NO_MDUPDATE
		OS_CFLAGS += $(NOMD_OS_CFLAGS)
	else
		OS_CFLAGS += $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
	endif
else
	CC		= cc
	CCC		= CC
	ASFLAGS		+= -Wa,-P
	OS_CFLAGS	+= $(NOMD_OS_CFLAGS)
endif

CPU_ARCH	= x86

MKSHLIB		= $(LD)
MKSHLIB		+= $(DSO_LDOPTS)
NOSUCHFILE	= /solx86-rm-f-sucks
RANLIB		= echo

# for purify
NOMD_OS_CFLAGS	+= -DSVR4 -DSYSV -D_REENTRANT -DSOLARIS -D__svr4__ -Di386

DSO_LDOPTS	+= -G
