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
# Config for all versions of Linux
#

CC = gcc
CCC = g++
CFLAGS +=  -Wall -Wno-format

RANLIB = echo
MKSHLIB = $(LD) -shared

#.c.o:
#      $(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = $(shell uname -m)
ifeq (86,$(findstring 86,$(CPU_ARCH)))
CPU_ARCH = x86
endif
GFX_ARCH = x

OS_CFLAGS = -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DLINUX
OS_LIBS = -lm -lc

ASFLAGS += -x assembler-with-cpp

ifeq ($(CPU_ARCH),alpha)

# Ask the C compiler on alpha linux to let us work with denormalized
# double values, which are required by the ECMA spec.

OS_CFLAGS += -mieee
endif
