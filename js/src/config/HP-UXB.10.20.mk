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
# Config stuff for HPUX
#

# CC = gcc
# CCC = g++
# CFLAGS +=  -Wall -Wno-format -fPIC

CC  = cc -Ae +Z
CCC = CC -Ae +a1 +eh +Z

RANLIB = echo
MKSHLIB = $(LD) -b

SO_SUFFIX = sl

#.c.o:
#	$(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = hppa
GFX_ARCH = x

OS_CFLAGS = -DXP_UNIX -DHPUX -DSYSV
OS_LIBS = -ldld

ifeq ($(OS_RELEASE),B.10)
PLATFORM_FLAGS		+= -DHPUX10 -Dhpux10
PORT_FLAGS		+= -DRW_NO_OVERLOAD_SCHAR -DHAVE_MODEL_H
ifeq ($(OS_VERSION),.10)
PLATFORM_FLAGS		+= -DHPUX10_10
endif
ifeq ($(OS_VERSION),.20)
PLATFORM_FLAGS		+= -DHPUX10_20
endif
ifeq ($(OS_VERSION),.30)
PLATFORM_FLAGS		+= -DHPUX10_30
endif
endif
