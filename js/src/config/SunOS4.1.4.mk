# -*- Mode: makefile -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#

#
# Config stuff for SunOS4.1
#

CC = gcc
CCC = g++
RANLIB = ranlib

#.c.o:
#	$(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = sparc
GFX_ARCH = x

# A pile of -D's to build xfe on sunos
MOZ_CFLAGS = -DSTRINGS_ALIGNED -DNO_REGEX -DNO_ISDIR -DUSE_RE_COMP \
	     -DNO_REGCOMP -DUSE_GETWD -DNO_MEMMOVE -DNO_ALLOCA \
	     -DBOGUS_MB_MAX -DNO_CONST

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS = -DXP_UNIX -Wall -Wno-format -DSW_THREADS -DSUNOS4 -DNEED_SYSCALL \
		 $(MOZ_CFLAGS)

OS_CFLAGS = $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
OS_LIBS = -ldl -lm

MKSHLIB = $(LD) -L$(MOTIF)/lib

HAVE_PURIFY = 1
MOTIF = /home/motif/usr
MOTIFLIB = -L$(MOTIF)/lib -lXm
INCLUDES += -I/usr/X11R5/include -I$(MOTIF)/include

NOSUCHFILE = /solaris-rm-f-sucks

LOCALE_MAP = $(DEPTH)/cmd/xfe/intl/sunos.lm

EN_LOCALE = en_US
DE_LOCALE = de
FR_LOCALE = fr
JP_LOCALE = ja
SJIS_LOCALE = ja_JP.SJIS
KR_LOCALE = ko
CN_LOCALE = zh
TW_LOCALE = zh_TW
I2_LOCALE = i2
IT_LOCALE = it
SV_LOCALE = sv
ES_LOCALE = es
NL_LOCALE = nl
PT_LOCALE = pt

LOC_LIB_DIR = /usr/openwin/lib/locale

BSDECHO	= echo

#
# These defines are for building unix plugins
#
BUILD_UNIX_PLUGINS = 1
DSO_LDOPTS =
DSO_LDFLAGS =
