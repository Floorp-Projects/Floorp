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
# Config stuff for HP-UX
#

include $(MOD_DEPTH)/config/UNIX.mk

DLL_SUFFIX	= sl

ifeq ($(NS_USE_GCC), 1)
	CC			        = gcc
	CCC			        = g++
	OS_CFLAGS		    =
	COMPILER_TAG        = _gcc
else
	CC				= cc -Ae
	CCC			        = CC -ext
	OS_CFLAGS           = +ESlit
endif

RANLIB			= echo

CPU_ARCH		= hppa

OS_CFLAGS		+= $(DSO_CFLAGS) -DHPUX -D$(CPU_ARCH) -D_HPUX_SOURCE

#
# The header netdb.h on HP-UX 9 does not declare h_errno.
# On 10.10 and 10.20, netdb.h declares h_errno only if
# _XOPEN_SOURCE_EXTENDED is defined.  So we need to declare
# h_errno ourselves.
#
ifeq ($(basename $(OS_RELEASE)),A.09)
OS_CFLAGS		+= -D_PR_NEED_H_ERRNO
endif
ifeq (,$(filter-out B.10.10 B.10.20,$(OS_RELEASE)))
OS_CFLAGS		+= -D_PR_NEED_H_ERRNO
endif

# Do we have localtime_r()?  Does it return 'int' or 'struct tm *'?
ifeq (,$(filter-out B.10.10 B.10.20,$(OS_RELEASE)))
OS_CFLAGS		+= -DHAVE_INT_LOCALTIME_R
endif
ifeq (,$(filter-out B.10.30 B.11.00,$(OS_RELEASE)))
OS_CFLAGS		+= -DHAVE_POINTER_LOCALTIME_R
endif

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
CLASSIC_NSPR = 1
endif

#
# On HP-UX 9, the default (and only) implementation strategy is
# classic nspr.
#
# On HP-UX 10.10 and 10.20, the default implementation strategy is
# pthreads (actually DCE threads).  Classic nspr is also available.
#
# On HP-UX 10.30 and 11.00, the default implementation strategy is
# pthreads.  Classic nspr and pthreads-user are also available.
#
ifeq ($(basename $(OS_RELEASE)),A.09)
OS_CFLAGS		+= -DHPUX9
DEFAULT_IMPL_STRATEGY = _EMU
endif

ifeq ($(OS_RELEASE),B.10.01)
OS_CFLAGS		+= -DHPUX10
DEFAULT_IMPL_STRATEGY = _EMU
endif

ifeq ($(OS_RELEASE),B.10.10)
OS_CFLAGS		+= -DHPUX10 -DHPUX10_10
DEFAULT_IMPL_STRATEGY = _PTH
endif

ifeq ($(OS_RELEASE),B.10.20)
OS_CFLAGS		+= -DHPUX10 -DHPUX10_20
ifneq ($(NS_USE_GCC), 1)
OS_CFLAGS		+= +DAportable
endif
DEFAULT_IMPL_STRATEGY = _PTH
endif

#
# On 10.30 and 11.00, we use the new ANSI C++ compiler aCC.
#

ifeq ($(OS_RELEASE),B.10.30)
ifneq ($(NS_USE_GCC), 1)
CCC			= /opt/aCC/bin/aCC -ext
OS_CFLAGS		+= +DAportable +DS1.1
endif
OS_CFLAGS		+= -DHPUX10 -DHPUX10_30
DEFAULT_IMPL_STRATEGY = _PTH
endif

# 11.00 is similar to 10.30.
ifeq ($(OS_RELEASE),B.11.00)
MODERN_HPUX=1
endif
ifeq ($(OS_RELEASE),B.11.11)
MODERN_HPUX=1
endif

ifdef MODERN_HPUX
	ifneq ($(NS_USE_GCC), 1)
		CCC			        = /opt/aCC/bin/aCC -ext
		ifeq ($(USE_64),1)
			OS_CFLAGS       += +DA2.0W +DS2.0 +DD64
			COMPILER_TAG    = _64
		else
			OS_CFLAGS       += +DAportable +DS2.0
ifeq ($(HAVE_CCONF), 1)
			COMPILER_TAG    =
else
			COMPILER_TAG    = _32
endif
		endif
	endif
OS_CFLAGS		+= -DHPUX10 -DHPUX11 -D_LARGEFILE64_SOURCE -D_PR_HAVE_OFF64_T
ifeq ($(HAVE_CCONF), 1)
DEFAULT_IMPL_STRATEGY =
else
DEFAULT_IMPL_STRATEGY = _PTH
endif
endif

ifeq ($(DEFAULT_IMPL_STRATEGY),_EMU)
CLASSIC_NSPR = 1
endif

ifeq ($(DEFAULT_IMPL_STRATEGY),_PTH)
USE_PTHREADS = 1
IMPL_STRATEGY = _PTH
ifeq ($(CLASSIC_NSPR),1)
USE_PTHREADS =
IMPL_STRATEGY = _EMU
endif
ifeq ($(PTHREADS_USER),1)
USE_PTHREADS =
IMPL_STRATEGY = _PTH_USER
endif
endif

ifeq ($(CLASSIC_NSPR),1)
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
endif

ifeq (,$(filter-out A.09 B.10,$(basename $(OS_RELEASE))))
DEFINES			+= -D_PR_NO_LARGE_FILES
endif

#
# To use the true pthread (kernel thread) library on 10.30 and
# 11.00, we should define _POSIX_C_SOURCE to be 199506L.
# The _REENTRANT macro is deprecated.
#

ifdef USE_PTHREADS
ifeq (,$(filter-out B.10.10 B.10.20,$(OS_RELEASE)))
OS_CFLAGS		+= -D_REENTRANT -D_PR_DCETHREADS
else
OS_CFLAGS		+= -D_POSIX_C_SOURCE=199506L -D_PR_HAVE_THREADSAFE_GETHOST
endif
endif

ifdef PTHREADS_USER
OS_CFLAGS		+= -D_POSIX_C_SOURCE=199506L
endif

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -b +h $(notdir $@)

# -fPIC or +Z generates position independent code for use in shared
# libraries.
ifeq ($(NS_USE_GCC), 1)
DSO_CFLAGS		= -fPIC
else
DSO_CFLAGS		= +Z
endif
