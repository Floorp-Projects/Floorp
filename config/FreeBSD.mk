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
# Config stuff for FreeBSD
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= freebsd
CPU_ARCH		:= x86
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/X11R6/include
G++INCLUDES		= -I/usr/include/g++
LOC_LIB_DIR		=
MOTIF			=
MOTIFLIB		=
OS_LIBS			=

# Don't define BSD, because it's already defined in /usr/include/sys/param.h.
PLATFORM_FLAGS		= -DFREEBSD -DBSDI -DBSDI_2 -D__386BSD__ -Di386 $(DSO_CFLAGS)
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
PORT_FLAGS		+= -D_PR_NEED_FAKE_POLL
else
OS_LIBS			= -lc
PORT_FLAGS		+= -D_PR_LOCAL_THREADS_ONLY
endif

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_CFLAGS		= -fpic
DSO_LDFLAGS		= 
DSO_LDOPTS		= -Bshareable
