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
# Config stuff for SNI SINIX-N (aka ReliantUNIX)
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= sinix
CPU_ARCH		:= mips
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/local/include
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/locale
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			= -lsocket -lnsl -lgen -lm -ldl -lresolv -lc -L/usr/ucblib -lucb

PLATFORM_FLAGS		= -DSNI -Dsinix
MOVEMAIL_FLAGS		= -DUSG
PORT_FLAGS		= -DSVR4 -DHAVE_FILIO_H -DNEED_S_ISSOCK -DNEED_TIMEVAL -DNEED_SELECT_H -DHAVE_LCHOWN -DNEED_S_ISLNK -DNEED_FCHMOD_PROTO -DNO_CDEFS_H -DSYS_BYTEORDER_H -DUSE_NODL_TABS -DMITSHM -DNO_MULTICAST -DHAVE_NETINET_IN_H -DHAVE_INT32_T
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/sinix.lm
EN_LOCALE		= en_US.88591
DE_LOCALE		= de_DE.88591
FR_LOCALE		= fr_FR.88591
JP_LOCALE		= ja_JP.EUC
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko_KR.euc
CN_LOCALE		= zh_CN.ugb
TW_LOCALE		= zh_TW.ucns
I2_LOCALE		= i2
IT_LOCALE		= it_IT.88591
SV_LOCALE		= sv_SV.88591
ES_LOCALE		= es_ES.88591
NL_LOCALE		= nl_NL.88591
PT_LOCALE		= pt_PT.88591

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

BSDECHO			= /usr/ucb/echo
EMACS			= /bin/true
WHOAMI			= /usr/ucb/whoami
PERL			= $(LOCAL_BIN)perl

######################################################################
# Other
######################################################################

ifdef NS_USE_NATIVE
CC			= cc
CCC			= CC
PLATFORM_FLAGS		+= -fullwarn -xansi
ifdef BUILD_OPT
OPTIMIZER		= -Olimit 4000
endif
else
PLATFORM_FLAGS		+= -pipe -Wall -Wno-format
ASFLAGS			+= -x assembler-with-cpp
ifdef BUILD_OPT
OPTIMIZER		= -O
else
OPTIMIZER		= -gdwarf
JAVA_OPTIMIZER		= -gdwarf
endif
endif

ifneq ($(USE_KERNEL_THREADS),1)
PORT_FLAGS		+= -DSW_THREADS
endif

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -G
DSO_LDFLAGS		= $(MOTIFLIB) -lXt -lX11 $(OS_LIBS)
