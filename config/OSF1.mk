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
# Config stuff for DEC OSF/1 (Digital UNIX)
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= dec
CPU_ARCH		:= alpha
GFX_ARCH		:= x

OS_INCLUDES		=
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		=
OS_LIBS			=

PLATFORM_FLAGS		= -taso -D_ALPHA_ -DIS_64 -DOSF1
MOVEMAIL_FLAGS		=
PORT_FLAGS		= -D_REENTRANT -DHAVE_LCHOWN -DNEED_CDEFS_H -DNTOHL_ENDIAN_H -DNEED_IOCTL_H -DMACHINE_ENDIAN_H -DHAVE_VA_LIST_STRUCT -DNEED_BYTE_ALIGNMENT -DMITSHM -DNEED_REALPATH -DHAVE_WAITID -DNEED_H_ERRNO -DNEED_SYS_TIME_H -DHAVE_SYSTEMINFO_H -DNEED_SYS_PARAM_H -DHAVE_INT32_T -DODD_VA_START -DHAVE_REMAINDER
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/osf1.lm
EN_LOCALE		= en_US.ISO8859-1
DE_LOCALE		= de_DE.ISO8859-1
FR_LOCALE		= fr_FR.ISO8859-1
JP_LOCALE		= ja_JP.eucJP
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko_KR.eucKR
CN_LOCALE		= zh_CN
TW_LOCALE		= zh_TW.eucTW
I2_LOCALE		= i2
IT_LOCALE		= it
SV_LOCALE		= sv
ES_LOCALE		= es
NL_LOCALE		= nl
PT_LOCALE		= pt

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(OS_RELEASE),V2)
PORT_FLAGS		+= -DNEED_TIME_R
else
OS_LIBS			+= -lrt -lc_r
endif

ifeq ($(OS_RELEASE),V3)
PLATFORM_FLAGS		+= -DOSF1V3
endif
ifeq ($(OS_RELEASE),V4)
PLATFORM_FLAGS		+= -DOSF1V4
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

AR			= ar rcl $@
CC			= cc -ieee_with_inexact -std
CCC			= cxx -ieee_with_inexact -x cxx -cfront
SHELL			= /usr/bin/ksh
WHOAMI			= /bin/whoami
UNZIP_PROG		= $(NS_BIN)unzip
ZIP_PROG		= $(NS_BIN)zip

######################################################################
# Other
######################################################################

ifdef BUILD_OPT
OPTIMIZER		+= -Olimit 4000
endif

ifeq ($(USE_KERNEL_THREADS),1)
ifdef NSPR20
PLATFORM_FLAGS		+= -pthread
OS_LIBS			+= -lpthread
else
PLATFORM_FLAGS		+= -threads
PORT_FLAGS		+= -DHW_THREADS
endif
else
PORT_FLAGS		+= -DSW_THREADS
endif

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -shared -all -expect_unresolved "*"
DSO_LDFLAGS		= -lXm -lXt -lX11 -lc
