# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
