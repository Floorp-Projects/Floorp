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
# Config stuff for BSDI BSD/386 and BSD/OS for x86
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= bsdi
CPU_ARCH		:= x86
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/X11/include
G++INCLUDES		= -I/usr/include/g++
LOC_LIB_DIR		= /usr/X11/lib
MOTIF			= $(NS_LIB)/Xm
MOTIFLIB		= -lXm
OS_LIBS			= -lcompat

PLATFORM_FLAGS		= -Wall -Wno-format -DBSDI -D__386BSD__ -DBSD -Di386
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSW_THREADS -DNEED_BSDREGEX -DNTOHL_ENDIAN_H -DUSE_NODL_TABS -DNEED_SYS_WAIT_H -DNO_TZNAME -DHAVE_NETINET_IN_H -DNO_INT64_T -DNEED_UINT_T -DHAVE_SYS_SELECT_H
PDJAVA_FLAGS		=

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

ifeq ($(OS_RELEASE),1.1)
PLATFORM_FLAGS		+= -DBSDI_1
PORT_FLAGS		+= -DNEED_UINT -DNEED_IOCTL_H -DNEED_REALPATH
endif
ifeq ($(OS_RELEASE),2.1)
PLATFORM_FLAGS		+= -DBSDI_2
PORT_FLAGS		+= -DHAVE_FILIO_H
UNZIP_PROG		= $(CONTRIB_BIN)unzip
endif
ifeq ($(OS_RELEASE),3.0)
PLATFORM_FLAGS		+= -DBSDI_2 -DBSDI_3
PORT_FLAGS		+= -DHAVE_FILIO_H
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

EMACS			= /usr/bin/true
PERL			= /usr/bin/perl
RANLIB			= /usr/bin/ranlib

######################################################################
# Other
######################################################################

