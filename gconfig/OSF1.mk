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
# Config stuff for DEC OSF/1
#

#
# The Bourne shell (sh) on OSF1 doesn't handle "set -e" correctly,
# which we use to stop LOOP_OVER_DIRS submakes as soon as any
# submake fails.  So we use the Korn shell instead.
#
SHELL = /usr/bin/ksh

include $(GDEPTH)/gconfig/UNIX.mk

DEFAULT_COMPILER = cc

CC         = cc
OS_CFLAGS += $(NON_LD_FLAGS) -std
CCC        = cxx
RANLIB     = /bin/true
CPU_ARCH   = alpha

ifdef BUILD_OPT
	OPTIMIZER += -Olimit 4000
endif

NON_LD_FLAGS += -ieee_with_inexact
OS_CFLAGS    += -DOSF1 -D_REENTRANT -taso

ifeq ($(USE_PTHREADS),1)
	OS_CFLAGS += -pthread
endif

ifeq ($(USE_IPV6),1)
	OS_CFLAGS += -D_PR_INET6
endif

# The command to build a shared library on OSF1.
MKSHLIB    += ld -shared -all -expect_unresolved "*"
DSO_LDOPTS += -shared
