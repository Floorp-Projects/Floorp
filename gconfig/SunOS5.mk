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
# Config stuff for SunOS5.x
#

include $(GDEPTH)/gconfig/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
	LOCAL_THREADS_ONLY = 1
	ifndef NS_USE_NATIVE
		NS_USE_GCC = 1
	endif
endif

#
# The default implementation strategy for Solaris is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
else
	ifeq ($(LOCAL_THREADS_ONLY),1)
		IMPL_STRATEGY = _LOCAL
		DEFINES += -D_PR_LOCAL_THREADS_ONLY
	else
		DEFINES += -D_PR_GLOBAL_THREADS_ONLY
	endif
endif

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
	IMPL_STRATEGY =
endif

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC         = gcc
	OS_CFLAGS += -Wall -Wno-format
	CCC        = g++
	CCC       += -Wall -Wno-format
	ASFLAGS	  += -x assembler-with-cpp
	ifdef NO_MDUPDATE
		OS_CFLAGS += $(NOMD_OS_CFLAGS)
	else
		OS_CFLAGS += $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
	endif
else
	CC         = cc
	CCC        = CC
	ASFLAGS   += -Wa,-P
	OS_CFLAGS += $(NOMD_OS_CFLAGS)
	ifndef BUILD_OPT
		OS_CFLAGS  += -xs
	endif

endif

INCLUDES   += -I/usr/dt/include -I/usr/openwin/include

RANLIB      = echo
CPU_ARCH    = sparc
OS_DEFINES += -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS

ifneq ($(LOCAL_THREADS_ONLY),1)
	OS_DEFINES		+= -D_REENTRANT
endif

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS += $(DSO_CFLAGS) $(OS_DEFINES) $(SOL_CFLAGS)

MKSHLIB  = $(LD)
MKSHLIB += $(DSO_LDOPTS)

# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
DSO_LDOPTS += -G

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifdef NS_USE_GCC
	DSO_CFLAGS += -fPIC
else
	DSO_CFLAGS += -KPIC
endif

HAVE_PURIFY  = 1
NOSUCHFILE   = /solaris-rm-f-sucks
