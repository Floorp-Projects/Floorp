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

include $(GDEPTH)/gconfig/UNIX.mk

#
# The default implementation strategy for Linux is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

CC			= gcc
CCC			= g++
RANLIB			= ranlib

DEFAULT_COMPILER = gcc

ifeq ($(OS_RELEASE),ppc)
	OS_REL_CFLAGS	= -DMACLINUX -DLINUX1_2
	CPU_ARCH	= ppc	
else
ifeq ($(OS_TEST),alpha)
        OS_REL_CFLAGS   = -D_ALPHA_ -DLINUX1_2
	CPU_ARCH	= alpha
else
	OS_REL_CFLAGS	= -mno-486 -DLINUX1_2 -Di386
	CPU_ARCH	= x86
endif
endif

ifeq ($(OS_RELEASE),2.0)
	OS_REL_CFLAGS	+= -DLINUX2_0
	MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
	ifdef BUILD_OPT
		OPTIMIZER	= -O2
	endif
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DLINUX -Dlinux -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR
OS_LIBS			+= -L/lib -ldl -lc

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT -D_PR_NEED_FAKE_POLL
endif

ARCH			= linux

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=

G++INCLUDES		= -I/usr/include/g++
