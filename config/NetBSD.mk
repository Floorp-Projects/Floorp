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

######################################################################
# Config stuff for NetBSD
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= netbsd
ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= x86
else
CPU_ARCH		:= $(OS_TEST)
endif
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/X11R6/include
G++INCLUDES		= -I/usr/include/g++
LOC_LIB_DIR		=
MOTIF			=
MOTIFLIB		=
OS_LIBS			=

OS_MINOR		= $(shell echo $(OS_VERSION) | cut -f2 -d.)

# Don't define BSD, because it's already defined in /usr/include/sys/param.h.
PLATFORM_FLAGS		= -DNETBSD $(DSO_CFLAGS)
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSW_THREADS -DNEED_UINT -DHAVE_LCHOWN -DNTOHL_ENDIAN_H -DHAVE_FILIO_H -DNEED_SYS_TIME_H -DNEED_UINT_T -DHAVE_BSD_FLOCK
PDJAVA_FLAGS		= -mx128m
OS_GPROF_FLAGS		= -pg
LD_FLAGS		= -L/usr/X11R6/lib -lXm

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/bsd386.lm
EN_LOCALE		= C
DE_LOCALE		= de_DE.ISO8859-1
FR_LOCALE		= fr_FR.ISO8859-1
JP_LOCALE		= ja
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko_KR.EUC
CN_LOCALE		= zh
TW_LOCALE		= zh
I2_LOCALE		= i2

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

DLL_SUFFIX		= so.1.0
EMACS			= /usr/bin/true
JAVA_PROG		= $(JAVA_BIN)java
RANLIB			= /usr/bin/ranlib

######################################################################
# Other
######################################################################

ifeq ($(USE_PTHREADS),1)
OS_LIBS			= -lc_r
else
OS_LIBS			= -lc
PORT_FLAGS		+= -D_PR_LOCAL_THREADS_ONLY
endif

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_CFLAGS		= -fpic
DSO_LDFLAGS		= 

#
# For NetBSD > 1.3, this can all be -shared.
#
ifneq (,$(filter alpha mips pmax,$(CPU_ARCH)))
DSO_LDOPTS		= -shared
else
DSO_LDOPTS		= -Bshareable
endif
