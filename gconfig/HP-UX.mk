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
# Config stuff for HP-UX
#

include $(GDEPTH)/gconfig/UNIX.mk

DEFAULT_COMPILER = cc

CPU_ARCH   = hppa
DLL_SUFFIX = sl
CCC        = CC
OS_CFLAGS  += -Ae $(DSO_CFLAGS) -DHPUX -D$(CPU_ARCH) -D_HPUX_SOURCE

ifeq ($(DEFAULT_IMPL_STRATEGY),_PTH)
	USE_PTHREADS = 1
	ifeq ($(CLASSIC_NSPR),1)
		USE_PTHREADS =
		IMPL_STRATEGY = _CLASSIC
	endif
	ifeq ($(PTHREADS_USER),1)
		USE_PTHREADS =
		IMPL_STRATEGY = _PTH_USER
	endif
endif

ifdef USE_PTHREADS
	OS_CFLAGS	+= -D_REENTRANT
endif
ifdef PTHREADS_USER
	OS_CFLAGS	+= -D_REENTRANT
endif

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -b
DSO_LDFLAGS		=

# +Z generates position independent code for use in shared libraries.
DSO_CFLAGS = +Z

HAVE_PURIFY		= 1
