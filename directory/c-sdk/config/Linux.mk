# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

######################################################################
# Config stuff for Linux (all architectures)
######################################################################

######################################################################
# Version-independent
######################################################################

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
CLASSIC_NSPR = 1
endif
endif

#
# The default implementation strategy for Linux is pthreads.
#
ifeq ($(CLASSIC_NSPR),1)
IMPL_STRATEGY		= _EMU
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
else
USE_PTHREADS		= 1
ifeq ($(HAVE_CCONF), 1)
IMPL_STRATEGY		= _glibc_PTH
else
IMPL_STRATEGY		= _PTH
endif
DEFINES			+= -D_REENTRANT
endif

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= x86
else
ifeq (,$(filter-out arm% sa110,$(OS_TEST)))
CPU_ARCH		:= arm
else
CPU_ARCH		:= $(OS_TEST)
endif
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

CC			= gcc
CCC			= g++
RANLIB			= ranlib

OS_INCLUDES		=
G++INCLUDES		= -I/usr/include/g++

PLATFORM_FLAGS		= -ansi -Wall -pipe -DLINUX -Dlinux -D_LARGEFILE64_SOURCE
PORT_FLAGS		= -D_POSIX_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -DHAVE_STRERROR

OS_CFLAGS		= $(DSO_CFLAGS) $(PLATFORM_FLAGS) $(PORT_FLAGS)

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(CPU_ARCH),alpha)
PLATFORM_FLAGS		+= -D_ALPHA_ -D__alpha -mieee
endif
ifeq ($(CPU_ARCH),x86)
PLATFORM_FLAGS		+= -Di386
endif
ifeq ($(CPU_ARCH),m68k)
#
# gcc on Linux/m68k either has a bug or triggers a code-sequence
# bug in the 68060 which causes gcc to crash.  The simplest way to
# avoid this is to enable a minimum level of optimization.
#
ifndef BUILD_OPT
OPTIMIZER		+= -O
endif
PLATFORM_FLAGS		+= -m68020-40
endif

#
# Linux 2.x has shared libraries.
#

MKSHLIB			= $(LD) $(DSO_LDOPTS) -soname $(notdir $@)
ifdef BUILD_OPT
OPTIMIZER		= -O2
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

######################################################################
# Other
######################################################################

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared
