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
# Config stuff for Linux
#

include $(DEPTH)/config/UNIX.mk

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= x86
else
ifeq (,$(filter-out armv4l sa110,$(OS_TEST)))
CPU_ARCH		:= arm
else
CPU_ARCH		:= $(OS_TEST)
endif
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

DEFAULT_COMPILER                = gcc
CC				= gcc
CXX				= g++
AS				= gcc -c
RANLIB			= ranlib
MKSHLIB			= $(CC) $(DSO_LDOPTS) -Wl,-soname,$(@:$(OBJDIR)/%.so=%.so)
MKMODULE		= ld -Ur -o $@

WARNING_CFLAG	= -Wall

# used by mkdepend
X11INCLUDES             =   -I/usr/X11R6/include
SYS_INCLUDES		=   -I$(subst libgcc.a,include, \
                                      $(shell $(CC) -print-libgcc-file-name))
SYS_INCLUDES		+=  -I$(subst libgcc.a,include, \
                                      $(shell $(CCC) -print-libgcc-file-name))
ifeq ($(CPU_ARCH),x86)
DEPENDFLAGS		+= -D__i386__
endif

OS_CFLAGS		= $(DSO_CFLAGS) -DLINUX -Dlinux
OS_CXXFLAGS		= $(OS_CFLAGS)
OS_ASFLAGS		= -DLINUX -Dlinux
OS_LDFLAGS		=
OS_LIBS			= -lm

DSO_CFLAGS		= -fPIC
DSO_LDFLAGS		= -Wl,export-dynamic
DSO_LDOPTS		= -shared

PERL			= perl

NSPR_THREAD_LIBS	= -lpthread
