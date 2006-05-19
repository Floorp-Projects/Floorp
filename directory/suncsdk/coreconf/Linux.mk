# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#
# Config stuff for Linux
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

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
endif
endif

ifeq ($(OS_RELEASE),2.0)
	OS_REL_CFLAGS	+= -DLINUX2_0
	MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
	ifdef BUILD_OPT
		OPTIMIZER	= -O2
	endif
endif

ifeq (86,$(findstring 86,$(OS_TEST)))
ifeq ($(USE_64),1)
	ARCH_FLAG		= -m64
	OS_REL_CFLAGS	= -DLINUX1_2 -Dx86-64
	CPU_ARCH		= x86_64
	CPU_ARCH_TAG    = _$(CPU_ARCH)
else
	ARCH_FLAG		= -m32
	OS_REL_CFLAGS	= -DLINUX1_2 -Di386
	CPU_ARCH		= x86
	CPU_ARCH_TAG    = _$(CPU_ARCH)
endif
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) $(ARCH_FLAG) -ansi -Wall -pipe -DLINUX -Dlinux -D_POSIX_SOURCE -D_BSD_SOURCE -DHAVE_STRERROR
OS_LIBS			= -L/lib -ldl -lc

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT -D_PR_NEED_FAKE_POLL
endif

ARCH			= linux

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=

G++INCLUDES		= -I/usr/include/g++
