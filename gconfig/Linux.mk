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
# Config stuff for Linux
#

include $(GDEPTH)/gconfig/UNIX.mk

ifdef MOZILLA_MOTIF_SEARCH_PATH
INCLUDES += -I$(MOZILLA_MOTIF_SEARCH_PATH)/include
else
INCLUDES += -I/usr/lesstif/include
endif

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

OS_REL_CFLAGS	+= -DLINUX2_0
MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
ifdef BUILD_OPT
	OPTIMIZER	= -O2
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DLINUX -Dlinux -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR

ifdef MOZILLA_MOTIF_SEARCH_PATH
OS_LIBS			+= -L$(MOZILLA_MOTIF_SEARCH_PATH)/lib
endif

OS_LIBS			+= -L/usr/X11R6/lib -L/lib -ldl -lc

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT -D_PR_NEED_FAKE_POLL
endif

ARCH			= linux

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=

G++INCLUDES		= -I/usr/include/g++

AR_ALL = -Wl,--whole-archive
AR_NONE = -Wl,--no-whole-archive
LINK_PROGRAM += -rdynamic
