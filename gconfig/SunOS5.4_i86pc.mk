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
