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

#
# Config stuff for DEC OSF/1
#

#
# The Bourne shell (sh) on OSF1 doesn't handle "set -e" correctly,
# which we use to stop LOOP_OVER_DIRS submakes as soon as any
# submake fails.  So we use the Korn shell instead.
#
SHELL			= /usr/bin/ksh

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
# Prior to OSF1 V4.0, classic nspr is the default (and only) implementation
# strategy.
#
# On OSF1 V4.0, pthreads is the default implementation strategy.
# Classic nspr is also available.
#
ifeq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
CLASSIC_NSPR = 1
endif

ifeq ($(CLASSIC_NSPR), 1)
	IMPL_STRATEGY = _EMU
	DEFINES += -D_PR_LOCAL_THREADS_ONLY
else
	USE_PTHREADS = 1
ifeq ($(HAVE_CCONF), 1)
	IMPL_STRATEGY =
else
	IMPL_STRATEGY = _PTH
endif
endif

ifeq ($(HAVE_CCONF), 1)
CC			= cc $(NON_LD_FLAGS)
else
CC			= cc $(NON_LD_FLAGS) -std1
endif

ifneq ($(OS_RELEASE),V2.0)
CC			+= -readonly_strings
endif
# The C++ compiler cxx has -readonly_strings on by default.
CCC			= cxx

RANLIB			= /bin/true

CPU_ARCH		= alpha

ifdef BUILD_OPT
OPTIMIZER		+= -Olimit 4000
endif

NON_LD_FLAGS		= -ieee_with_inexact

OS_CFLAGS		= -DOSF1 -D_REENTRANT

ifeq ($(HAVE_CCONF), 1)
OS_CFLAGS		+= -DIS_64 -DOSF1V4D -DOSF1
endif

ifneq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
OS_CFLAGS		+= -DOSF1_HAVE_MACHINE_BUILTINS_H
endif

ifeq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
OS_CFLAGS		+= -DHAVE_INT_LOCALTIME_R
else
OS_CFLAGS		+= -DHAVE_POINTER_LOCALTIME_R
endif

ifeq (,$(filter-out V4.0%,$(OS_RELEASE)))
OS_CFLAGS		+= -DOSF1V4_MAP_PRIVATE_BUG
endif

ifeq ($(USE_PTHREADS),1)
OS_CFLAGS		+= -pthread
ifneq (,$(filter-out V2.0 V3.2,$(OS_RELEASE)))
OS_CFLAGS		+= -D_PR_HAVE_THREADSAFE_GETHOST
endif
endif

# The command to build a shared library on OSF1.
MKSHLIB = ld -shared -all -expect_unresolved "*" -soname $(notdir $@)
DSO_LDOPTS		= -shared
