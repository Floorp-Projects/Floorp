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
# Config stuff for IRIX
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= irix
CPU_ARCH		:= mips
GFX_ARCH		:= x

OS_INCLUDES		=
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		=
OS_LIBS			=

PLATFORM_FLAGS		= -DIRIX -DIRIX$(OS_RELEASE)$(subst .,_,$(OS_VERSION))
MOVEMAIL_FLAGS		=
PORT_FLAGS		= -DSVR4 -DHAVE_LCHOWN -DHAVE_SIGNED_CHAR -DHAVE_FILIO_H -DHAS_PGNO_T -DMITSHM -DHAVE_WAITID -DNEED_VBASE -DNEED_SYS_TIME_H -DHAVE_SYSTEMINFO_H -DNO_JNI_STUBS -D_MIPS_SIM_ABI32
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/irix.lm
EN_LOCALE		= en_US
DE_LOCALE		= de
FR_LOCALE		= fr
JP_LOCALE		= ja_JP.EUC
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko_KR.euc
CN_LOCALE		= zh_CN.ugb
TW_LOCALE		= zh_TW.ucns
I2_LOCALE		= i2
IT_LOCALE		= it
SV_LOCALE		= sv
ES_LOCALE		= es
NL_LOCALE		= nl
PT_LOCALE		= pt

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(OS_RELEASE),6)
#
# The "-woff 131" silences the really noisy 6.x ld's warnings about
# having multiply defined weak symbols.
#
# The "-woff 3247" silences complaints about the "#pragma segment"
# stuff strewn all over db (apparently for Macintoshes).
#
NO_NOISE		= -woff 131
PLATFORM_FLAGS		+= -multigot -Wl,-nltgot,170
PORT_FLAGS		+= -DNO_UINT32_T -DNO_INT64_T -DNEED_BSD_TYPES
SHLIB_LD_OPTS		= -no_unresolved
ifeq ($(AWT_11),1)
JAVAC_ZIP		= $(NS_LIB)/rt.jar:$(NS_LIB)/dev.jar:$(NS_LIB)/i18n.jar:$(NS_LIB)/tiny.jar
endif
endif

ifndef NS_USE_GCC
CC			= cc
CCC			= CC -woff 3247
endif

ifeq ($(OS_RELEASE)$(OS_VERSION),5.3)
PERL			= $(LOCAL_BIN)perl5
ifndef NS_USE_GCC
XGOT_FLAG		= -xgot
#
# Use gtscc to unbloat the C++ global count.
#
ifdef USE_GTSCC
ifndef NO_GTSCC
XGOT_FLAG		=
CCC			= $(DIST)/bin/gtscc $(GTSCC_CC_OPTIONS) -gtsfile $(DEPTH)/config/$(OBJDIR)/db.gts -gtsrootdir $(DEPTH)
ifeq ($(findstring modules/,$(SRCDIR)),modules/)
CC			= $(DIST)/bin/gtscc $(GTSCC_CC_OPTIONS) -gtsfile $(DEPTH)/config/$(OBJDIR)/db.gts -gtsrootdir $(DEPTH)
endif
ifeq ($(findstring sun-java/,$(SRCDIR)),sun-java/)
CC			= $(DIST)/bin/gtscc $(GTSCC_CC_OPTIONS) -gtsfile $(DEPTH)/config/$(OBJDIR)/db.gts -gtsrootdir $(DEPTH)
endif
endif
endif
endif
PLATFORM_FLAGS		+= $(XGOT_FLAG)
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

WHOAMI			= /bin/whoami
UNZIP_PROG		= $(NS_BIN)unzip
ZIP_PROG		= $(NS_BIN)zip

######################################################################
# Other
######################################################################

ifdef NS_USE_GCC
PLATFORM_FLAGS		+= -Wall -Wno-format
ASFLAGS			+= -x assembler-with-cpp
ifdef BUILD_OPT
OPTIMIZER		= -O6
endif
else
PLATFORM_FLAGS		+= -32 -fullwarn -xansi -DIRIX_STARTUP_SPEEDUPS
ifdef BUILD_OPT
OPTIMIZER		= -O -Olimit 4000
endif
endif

ifndef NO_MDUPDATE
MDUPDATE_FLAGS		= -MDupdate $(DEPENDENCIES)
endif

ifeq ($(USE_KERNEL_THREADS),1)
PORT_FLAGS		+= -DHW_THREADS -D_SGI_MP_SOURCE
else
PORT_FLAGS		+= -DSW_THREADS
endif

#
# The "o32" calling convention is the default for 5.3 and 6.2.
# According to the SGI dudes, they will migrate to "n32" for 6.5.
# What will we do then? 
# If we want to do the same, simply uncomment the line below ..
#
#PORT_FLAGS		+= -D_MIPS_SIM_NABI32

#
# To get around SGI's problems with the Asian input method.
#
MAIL_IM_HACK		= *Mail*preeditType:none
NEWS_IM_HACK		= *News*preeditType:none

#
# An nm command which generates an output like:
# archive.a:object.o: 0000003 T symbol
#
NM_PO			= nm -Bpo

HAVE_PURIFY		= 1
MUST_BOOTLEG_ALLOCA	= 1
BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(NO_NOISE) $(SHLIB_LD_OPTS) -shared -soname $(@:$(OBJDIR)/%.so=%.so)

DSO_LDOPTS		= -elf -shared -all
DSO_LDFLAGS		= -nostdlib -L/lib -L/usr/lib  -L/usr/lib -lXm -lXt -lX11 -lgen

ifdef DSO_BACKEND
DSO_LDOPTS		+= -soname $(DSO_NAME)
endif

