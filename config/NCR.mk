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
# Config stuff for NCR SVR4 MP-RAS
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= ncr
CPU_ARCH		:= x86
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/X/include
G++INCLUDES		=
LOC_LIB_DIR		= /usr/X/lib
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			=

PLATFORM_FLAGS		= -DNCR -D_ATT4 -Di386
MOVEMAIL_FLAGS		= -DUSG -DHAVE_STRERROR
PORT_FLAGS		= -DSVR4 -DSYSV -DSW_THREADS -DHAVE_FILIO_H -DHAVE_LCHOWN -DNEED_S_ISLNK -DNEED_S_ISSOCK -DSYS_ENDIAN_H -DSYS_BYTEORDER_H -DUSE_NODL_TABS -DMITSHM -DHAVE_WAITID -DHAVE_NETINET_IN_H -DHAVE_REMAINDER -DHAVE_SYS_BITYPES_H -DPRFSTREAMS_BROKEN
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		=
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

EMACS			= /bin/true
PERL			= $(LOCAL_BIN)perl
WHOAMI			= /usr/ucb/whoami

######################################################################
# Other
######################################################################

ifdef NS_USE_NATIVE
CC			= cc
CCC			= ncc
PLATFORM_FLAGS		+= -Hnocopyr
OS_LIBS			+= -L/opt/ncc/lib
else
PLATFORM_FLAGS		+= -Wall
OS_LIBS			+= -lsocket -lnsl -lresolv -ldl
endif

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -G
DSO_LDFLAGS		= -lXm -lXt -lX11 -lsocket -lnsl -lgen
