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
# Config stuff for SunOS5.5
#

AS = as
ifndef NS_USE_NATIVE
  CC = gcc
  CCC = g++
  CFLAGS +=  -Wall -Wno-format
else
  CC = cc
  CCC = CC
  CFLAGS += -mt -Kpic
#  LD = CC
endif

RANLIB = echo

#.c.o:
#	$(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = sparc
GFX_ARCH = x

OS_CFLAGS = -DXP_UNIX -DSVR4 -DSYSV -DSOLARIS
OS_LIBS = -lsocket -lnsl -ldl

ASFLAGS	        += -P -L -K PIC -D_ASM -D__STDC__=0

HAVE_PURIFY = 1

NOSUCHFILE = /solaris-rm-f-sucks

ifeq ($(OS_CPUARCH),sun4u)	# ultra sparc?
ifeq ($(CC),gcc)		# using gcc?
ifndef JS_NO_ULTRA		# do we want ultra?
ifdef JS_THREADSAFE		# only in thread-safe mode
DEFINES 	+= -DULTRA_SPARC
DEFINES         += -Wa,-xarch=v8plus,-DULTRA_SPARC
else
ASFLAGS         += -xarch=v8plus -DULTRA_SPARC
endif
endif
endif
endif

MKSHLIB = $(LD) -G
