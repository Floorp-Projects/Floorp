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
# Config stuff for IRIX
#
CPU_ARCH = mips
GFX_ARCH = x

RANLIB = /bin/true

#NS_USE_GCC = 1

ifdef NS_USE_GCC
CC = gcc
CCC = g++
AS = $(CC) -x assembler-with-cpp
ODD_CFLAGS = -Wall -Wno-format
ifdef BUILD_OPT
OPTIMIZER = -O6
endif
else
ifeq ($(OS_RELEASE),6.2)
CC	= cc -32 -DIRIX6_2
endif
ifeq ($(OS_RELEASE),6.3)
CC	= cc -32 -DIRIX6_3
endif
CCC = CC
ODD_CFLAGS = -fullwarn -xansi
ifdef BUILD_OPT
OPTIMIZER += -Olimit 4000
endif
endif

# For purify
HAVE_PURIFY = 1
PURE_OS_CFLAGS = $(ODD_CFLAGS) -DXP_UNIX -DSVR4 -DSW_THREADS -DIRIX

OS_CFLAGS = $(PURE_OS_CFLAGS) -MDupdate $(DEPENDENCIES)

BSDECHO	= echo
MKSHLIB = $(LD) -shared
