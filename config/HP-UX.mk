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
# Config stuff for HP-UX
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= hpux
CPU_ARCH		:= hppa
GFX_ARCH		:= x

OS_INCLUDES		= -I/usr/include/X11R$(X11_REV) -I/usr/contrib/X11R$(X11_REV)/include -I/usr/include/Motif$(MOTIF_REV)_R$(X11_REV) -I/usr/include/Motif$(MOTIF_REV)
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		=
MOTIF_REV		= 1.2
OS_LIBS			= -ldld

PLATFORM_FLAGS		= $(DSO_CFLAGS) -DHPUX -Dhpux -DHPUX$(subst .,,$(subst .0,,$(suffix $(OS_RELEASE)))) -DHPUX$(subst .,,$(subst .0,,$(suffix $(OS_RELEASE))))$(subst .,_,$(OS_VERSION)) -D$(CPU_ARCH) $(ADDITIONAL_CFLAGS)
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -D_HPUX_SOURCE -DSW_THREADS -DNO_SIGNED -DNO_FNDELAY -DHAVE_ODD_SELECT -DNO_CDEFS_H -DNEED_IOCTL_H -DNEED_MATH_H -DUSE_NODL_TABS -DMITSHM -DNEED_SYS_WAIT_H -DHAVE_INT32_T -DNEED_UINT_T -DNEED_H_ERRNO
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/hpux.lm
EN_LOCALE		= american.iso88591
DE_LOCALE		= german.iso88591
FR_LOCALE		= french.iso88591
JP_LOCALE		= japanese.euc
SJIS_LOCALE		= japanese
KR_LOCALE		= korean
CN_LOCALE		= chinese-s
TW_LOCALE		= chinese-t.big5
I2_LOCALE		= i2
IT_LOCALE		= it
SV_LOCALE		= sv
ES_LOCALE		= es
NL_LOCALE		= nl
PT_LOCALE		= pt

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(OS_RELEASE),A.09)
OS_LIBS			+= -L/lib/pa1.1 -lm
ifndef NS_USE_GCC
NO_INLINE		= +d
endif
else
PORT_FLAGS		+= -DHAVE_SNPRINTF
OS_LIBS			+= -lm
endif

ifeq ($(OS_RELEASE),B.10)
PORT_FLAGS		+= -DRW_NO_OVERLOAD_SCHAR -DHAVE_MODEL_H
JAVA_PROG		= $(CONTRIB_BIN)java
endif

ifeq ($(OS_RELEASE),B.11)
PLATFORM_FLAGS		+= -DHPUX10
MOTIF_REV		= 2.1
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

ifndef NS_USE_GCC
CC			= cc -Ae
CCC			= CC -Aa +a1 $(NO_INLINE)
endif

BSDECHO			= $(DIST)/bin/bsdecho
DLL_SUFFIX		= sl
EMACS			= /bin/true

ifdef NETSCAPE_HIERARCHY
PERL			= perl5
endif

ifdef BUILD_OPT
ifdef NS_USE_GCC
OPTIMIZER		= -O
else
OPTIMIZER		= -O +Onolimit
endif
endif

######################################################################
# Other
######################################################################

ifdef MOZ_USE_X11R6
X11_REV			= 6
else
X11_REV			= 5
endif

ifdef SERVER_BUILD
ifndef NS_USE_GCC                                                 
PLATFORM_FLAGS		+= +DA1.0                           
endif
PLATFORM_FLAGS		+= -Wl,-E
endif

ELIBS_CFLAGS		= -g -DHAVE_STRERROR

HAVE_PURIFY		= 1
MUST_BOOTLEG_ALLOCA	= 1
BUILD_UNIX_PLUGINS	= 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

DSO_LDOPTS		= -b
DSO_LDFLAGS		=

ifdef NS_USE_GCC
DSO_CFLAGS		= -fPIC
else
DSO_CFLAGS		= +Z
endif
