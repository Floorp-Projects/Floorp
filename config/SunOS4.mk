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
# Config stuff for SunOS4.1.x
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= sunos
CPU_ARCH		:= sparc
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/X11R5/include -I$(MOTIF)/include
G++INCLUDES		=
LOC_LIB_DIR		= /usr/openwin/lib/locale
MOTIF			= /home/motif/usr
MOTIFLIB		= -L$(MOTIF)/lib -lXm
OS_LIBS			= -ldl -lm

PLATFORM_FLAGS		= -Wall -Wno-format -DSUNOS4
MOVEMAIL_FLAGS		=
PORT_FLAGS		= -DSW_THREADS -DNEED_SYSCALL -DSTRINGS_ALIGNED -DNO_REGEX -DNO_ISDIR -DUSE_RE_COMP -DNO_REGCOMP -DUSE_GETWD -DNO_MEMMOVE -DNO_ALLOCA -DBOGUS_MB_MAX -DNO_CONST -DHAVE_ODD_SEND -DHAVE_ODD_IOCTL -DHAVE_FILIO_H -DMITSHM -DNEED_SYS_WAIT_H -DNO_TZNAME -DNEED_SYS_TIME_H -DNO_MULTICAST -DHAVE_INT32_T -DNEED_UINT_T -DUSE_ODD_SSCANF -DUSE_ODD_SPRINTF -DNO_IOSTREAM_H
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/sunos.lm
EN_LOCALE		= en_US
DE_LOCALE		= de
FR_LOCALE		= fr
JP_LOCALE		= ja
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko
CN_LOCALE		= zh
TW_LOCALE		= zh_TW
I2_LOCALE		= i2
IT_LOCALE		= it
SV_LOCALE		= sv
ES_LOCALE		= es
NL_LOCALE		= nl
PT_LOCALE		= pt

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

DLL_SUFFIX		= so.1.0
PERL			= $(LOCAL_SUN4)perl
RANLIB			= /bin/ranlib
TAR			= /usr/bin/tar
WHOAMI			= /usr/ucb/whoami
UNZIP_PROG		= $(NS_BIN)unzip
ZIP_PROG		= $(NS_BIN)zip

######################################################################
# Other
######################################################################

ifndef NO_MDUPDATE
MDUPDATE_FLAGS		= -MDupdate $(DEPENDENCIES)
endif

HAVE_PURIFY		= 1
MUST_BOOTLEG_ALLOCA	= 1
BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) -L$(MOTIF)/lib

DSO_LDOPTS		=
DSO_LDFLAGS		=
