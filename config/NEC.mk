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
# Config stuff for NEC EWS-UX/V
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= nec
CPU_ARCH		:= mips
GFX_ARCH		:= x

OS_INCLUDES		=
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		= -L/usr/lib -lXm
OS_LIBS			= -lsocket -lnsl -ldl -L/usr/ucblib -lc -lucb

PLATFORM_FLAGS		= -DNEC -Dnec_ews -DNECSVR4 -D__SVR4
MOVEMAIL_FLAGS		= -DUSG -DHAVE_STRERROR
PORT_FLAGS		= -DSVR4 -DSW_THREADS -DHAVE_FILIO_H -DHAVE_LCHOWN -DNEED_S_ISLNK -DNEED_CDEFS_H -DNO_LONG_LONG -DSYS_BYTEORDER_H -DMITSHM -DHAVE_NETINET_IN_H -DHAVE_ARPA_NAMESER_H
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/nec.lm
EN_LOCALE		= C
DE_LOCALE		= de_DE.ISO8859-1
FR_LOCALE		= fr_FR.ISO8859-1
JP_LOCALE		= ja_JP.EUC
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko_KR.EUC
CN_LOCALE		= zh_CN.ugb
TW_LOCALE		= zh_TW.ucns
I2_LOCALE		= i2

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

ifndef NS_USE_GCC
CC			= $(DEPTH)/build/hcc -Xa -KGnum=0 -KOlimit=4000
endif

BSDECHO			= /usr/ucb/echo
EMACS			= /bin/true
PERL			= $(LOCAL_BIN)perl
WHOAMI			= /usr/ucb/whoami

######################################################################
# Other
######################################################################

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -G
DSO_LDFLAGS		=
