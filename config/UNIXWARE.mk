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
# Config stuff for SCO UnixWare
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= sco_uw
CPU_ARCH		:= x86
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/X/include
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			= -lsocket -lc /usr/ucblib/libucb.a

PLATFORM_FLAGS		= -DUNIXWARE -Di386
MOVEMAIL_FLAGS		= -DUSG -DHAVE_STRERROR
PORT_FLAGS		= -DSVR4 -DSYSV -DSW_THREADS -DHAVE_FILIO_H -DHAVE_ODD_ACCEPT -DNEED_S_ISLNK -DLAME_READDIR -DNO_CDEFS_H -DNO_LONG_LONG -DNEED_S_ISSOCK -DSYS_BYTEORDER_H -DUSE_NODL_TABS -DMOTIF_WARNINGS_UPSET_JAVA -DMITSHM -DNEED_SYS_TIME_H -DNO_MULTICAST -DHAVE_NETINET_IN_H -DHAVE_REMAINDER -DHAVE_INT32_T
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/unixware.lm
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

ifeq ($(OS_RELEASE),5)
PLATFORM_FLAGS		+= -DUnixWare -DUNIXWARE5
PORT_FLAGS		+= -DSVR5 -D_SIMPLE_R

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS		= -G
DSO_LDFLAGS		= -nostdlib -L/lib -L/usr/lib -L/usr/X/lib -lXm -lXt -lX11 -lgen
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

CC			= $(DEPTH)/build/hcc
CCC			= $(DEPTH)/build/hcpp
EMACS			= /bin/true
WHOAMI			= /usr/ucb/whoami
PERL			= $(LOCAL_BIN)perl

######################################################################
# Other
######################################################################

