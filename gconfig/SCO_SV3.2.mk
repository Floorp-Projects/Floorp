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
# Config stuff for SCO Unix for x86.
#

include $(GDEPTH)/gconfig/UNIX.mk

DEFAULT_COMPILER = cc

CC         = cc
OS_CFLAGS += -b elf -KPIC
CCC        = g++
CCC       += -b elf -DPRFSTREAMS_BROKEN -I/usr/local/lib/g++-include
# CCC      = $(GDEPTH)/build/hcpp
# CCC     += +.cpp +w
RANLIB     = /bin/true

#
# -DSCO_PM - Policy Manager AKA: SCO Licensing
# -DSCO - Changes to Netscape source (consistent with AIX, LINUX, etc..)
# -Dsco - Needed for /usr/include/X11/*
#
OS_CFLAGS   += -DSCO_SV -DSYSV -D_SVID3 -DHAVE_STRERROR -DSW_THREADS -DSCO_PM -DSCO -Dsco
#OS_LIBS     += -lpmapi -lsocket -lc
MKSHLIB      = $(LD)
MKSHLIB     += $(DSO_LDOPTS)
XINC         = /usr/include/X11
MOTIFLIB    += -lXm
INCLUDES    += -I$(XINC)
CPU_ARCH     = x86
GFX_ARCH     = x
ARCH         = sco
LOCALE_MAP   = $(GDEPTH)/cmd/xfe/intl/sco.lm
EN_LOCALE    = C
DE_LOCALE    = de_DE.ISO8859-1
FR_LOCALE    = fr_FR.ISO8859-1
JP_LOCALE    = ja
SJIS_LOCALE  = ja_JP.SJIS
KR_LOCALE    = ko_KR.EUC
CN_LOCALE    = zh
TW_LOCALE    = zh
I2_LOCALE    = i2
LOC_LIB_DIR  = /usr/lib/X11
NOSUCHFILE   = /solaris-rm-f-sucks
BSDECHO      = /bin/echo

#
# These defines are for building unix plugins
#
BUILD_UNIX_PLUGINS  = 1
#DSO_LDOPTS         += -b elf -G -z defs
DSO_LDOPTS         += -b elf -G
DSO_LDFLAGS        += -nostdlib -L/lib -L/usr/lib -lXm -lXt -lX11 -lgen

# Used for Java compiler
EXPORT_FLAGS += -W l,-Bexport
