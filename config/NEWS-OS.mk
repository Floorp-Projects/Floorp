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
# Config stuff for Sony NEWS-OS
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= sony
CPU_ARCH		:= mips
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/include
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		= -L/usr/lib -lXm
OS_LIBS			= -lsocket -lnsl -lgen -lresolv

PLATFORM_FLAGS		= -Xa -fullwarn -DSONY
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSYSV -DSVR4 -D__svr4 -D__svr4__ -DSW_THREADS -DHAVE_INT32_T -DHAVE_STDDEF_H -DHAVE_STDLIB_H -DHAVE_FILIO_H -DSYS_BYTEORDER_H -DNO_CDEFS_H -DHAVE_LCHOWN -DHAS_PGNO_T -DNO_MULTICAST
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		=
EN_LOCALE		= C
DE_LOCALE		= de_DE.ISO8859-1
FR_LOCALE		= fr_FR.ISO8859-1
JP_LOCALE		= ja.JP.EUC
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

BSDECHO			= /usr/ucb/echo
CC			= cc
CCC			= CC
EMACS			= /bin/true
PERL			= /bin/true

######################################################################
# Other
######################################################################

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)
 
DSO_LDOPTS		= -G
DSO_LDFLAGS		=
