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
# Config stuff for Data General DG/UX
######################################################################
#
# Initial DG/UX port by Marc Fraioli <fraioli@dg-rtp.dg.com>
#
######################################################################
# Version-independent
######################################################################

ARCH			:= dgux
CPU_ARCH		:= x86
GFX_ARCH		:= x

OS_INCLUDES		= 
G++INCLUDES		=
LOC_LIB_DIR		= 
MOTIF			=
MOTIFLIB		= -lXm -lXt -lXmu -lX11
OS_LIBS			= -lgen -lresolv -lsocket -lnsl 

PLATFORM_FLAGS		= -DDGUX -Di386 -D_DGUX_SOURCE -D_POSIX4A_DRAFT6_SOURCE
MOVEMAIL_FLAGS		= -DUSG -DHAVE_STRERROR
PORT_FLAGS		= -DSVR4 -DSYSV -DNO_CDEFS_H -DNEED_S_ISSOCK -DSYS_BYTEORDER_H -DUSE_NODL_TABS -DMOTIF_WARNINGS_UPSET_JAVA -DMITSHM -DNO_MULTICAST -DHAVE_NETINET_IN_H -DHAVE_REMAINDER 
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/dgux.lm
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
PERL			= /usr/bin/perl
ifdef BUILD_OPT
OPTIMIZER		= -O2
else
OPTIMIZER		=
endif

######################################################################
# Other
######################################################################

