# -*- Mode: makefile -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#

#
# Config for all versions of Linux
#

CC = gcc
CCC = g++
CFLAGS +=  -Wall -Wno-format
OS_CFLAGS = -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE

RANLIB = echo
MKSHLIB = $(LD) -shared $(XMKSHLIBOPTS)

#.c.o:
#      $(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = $(shell uname -m)
ifeq (86,$(findstring 86,$(CPU_ARCH)))
CPU_ARCH = x86
OS_CFLAGS+= -DX86_LINUX

ifeq (gcc, $(CC))
# if using gcc on x86, check version for opt bug 
# (http://bugzilla.mozilla.org/show_bug.cgi?id=24892)
GCC_VERSION := $(shell gcc -v 2>&1 | grep version | awk '{ print $$3 }')
GCC_LIST:=$(sort 2.91.66 $(GCC_VERSION) )

ifeq (2.91.66, $(firstword $(GCC_LIST)))
CFLAGS+= -DGCC_OPT_BUG
endif
endif

endif
GFX_ARCH = x

OS_LIBS = -lm -lc

ASFLAGS += -x assembler-with-cpp


ifeq ($(CPU_ARCH),alpha)

# Ask the C compiler on alpha linux to let us work with denormalized
# double values, which are required by the ECMA spec.

OS_CFLAGS += -mieee
endif

# Use the editline library to provide line-editing support.
JS_EDITLINE = 1
