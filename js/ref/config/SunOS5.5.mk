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
# Config stuff for SunOS5.5
#
AS = as
CC = gcc -Wall -Wno-format
CCC = g++ -Wall -Wno-format

#CC = /opt/SUNWspro/SC3.0.1/bin/cc
RANLIB = echo

#.c.o:
#	$(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = sparc
GFX_ARCH = x

OS_CFLAGS = -DXP_UNIX -DSVR4 -DSYSV -DSOLARIS -D_SVID_GETTOD
OS_LIBS = -lsocket -lnsl -ldl

ASFLAGS	        += -P -L -K PIC -D_ASM -D__STDC__=0

HAVE_PURIFY = 1

NOSUCHFILE = /solaris-rm-f-sucks

ifndef JS_NO_ULTRA
ULTRA_OPTIONS := -xarch=v8plus -DULTRA_SPARC
else
ULTRA_OPTIONS := -xarch=v8
endif

ifeq ($(OS_CPUARCH),sun4u)
ASFLAGS         += $(ULTRA_OPTIONS)
ifeq ($(findstring gcc,$(CC)),gcc)
DEFINES         += -Wa,$(ULTRA_OPTIONS)
endif
else
ifeq ($(OS_CPUARCH),sun4m)
ASFLAGS         += -xarch=v8
ifeq ($(findstring gcc,$(CC)),gcc)
DEFINES         += -Wa,-xarch=v8
endif
endif
endif
