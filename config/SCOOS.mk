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
# Config stuff for SCO OpenServer
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= sco_os
CPU_ARCH		:= x86
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/include/X11
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			= -lpmapi -lsocket

PLATFORM_FLAGS		= -DSCO -Dsco -DSCO_SV -Di386
MOVEMAIL_FLAGS		= -DUSG -DHAVE_STRERROR
PORT_FLAGS		= -DSYSV -DSW_THREADS -DNO_SIGNED -DNEED_SOCKET_H -DNEED_S_ISLNK -DNO_LONG_LONG -DNEED_S_ISSOCK -DNEED_MATH_H -DSYS_BYTEORDER_H -DHAVE_BITYPES_H -DUSE_NODL_TABS -DMOTIF_WARNINGS_UPSET_JAVA -DMITSHM -DNO_ID_T -DHAVE_WAITID -DHAVE_SYS_NETINET_IN_H -DHAVE_REMAINDER -DNEED_SYS_SELECT_H -DHAVE_SYS_BITYPES_H -DNEED_H_ERRNO
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/sco.lm
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

BSDECHO			= /bin/echo
CC			= cc -b elf -K pic
CCC			= $(DEPTH)/build/hcpp +.cpp +d
EMACS			= /bin/true
WHOAMI			= $(LOCAL_BIN)whoami
UNZIP_PROG		= $(CONTRIB_BIN)unzip
ZIP_PROG		= $(CONTRIB_BIN)zip

######################################################################
# Other
######################################################################

#
# -DSCO_PM - Policy Manager AKA: SCO Licensing
# Only (supposedly) needed for the RTM builds.
#
ifdef NEED_SCO_PM
PLATFORM_FLAGS		+= -DSCO_PM
endif

BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -G -b elf -d y
DSO_LDFLAGS		=
