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
# Config stuff for SunOS 5.x for SPARC and x86
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= solaris
ifeq ($(OS_TEST),i86pc)
CPU_ARCH		:= x86
else
CPU_ARCH		:= sparc
endif
GFX_ARCH		:= x

OS_INCLUDES		= -I$(MOTIF)/include -I/usr/openwin/include
G++INCLUDES		=
LOC_LIB_DIR		= /usr/openwin/lib/locale
MOTIF			= /usr/dt
MOTIFLIB		= -lXm
OS_LIBS			= $(THREAD_LIB) -lposix4 $(RESOLV_LIB) -lsocket -lnsl -ldl


PLATFORM_FLAGS		= $(DSO_CFLAGS) -DSOLARIS -D__svr4 -D__svr4__
MOVEMAIL_FLAGS		= -DUSG
PORT_FLAGS		= -DSVR4 -DSYSV -DHAVE_WEAK_IO_SYMBOLS -DHAVE_FILIO_H -DHAVE_LCHOWN -DNEED_CDEFS_H -DMITSHM -DHAVE_WAITID -DHAVE_FORK1 -DHAVE_REMAINDER -DHAVE_SYSTEMINFO_H -DHAVE_INT32_T -DNO_JNI_STUBS -DHAVE_QSORT -DBROKEN_QSORT
PDJAVA_FLAGS		=

ifdef USE_AUTOCONF
OS_CFLAGS		= $(DSO_CFLAGS) #-DSOLARIS
else
OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)
endif

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/sunos.lm
EN_LOCALE		= en_US
DE_LOCALE		= de
FR_LOCALE		= fr
JP_LOCALE		= ja
SJIS_LOCALE		= ja_JP.SJIS
KR_LOCALE		= ko
CN_LOCALE		= zh
TW_LOCALE		= zh_TW
I2_LOCALE		= i2
IT_LOCALE		= it
SV_LOCALE		= sv
ES_LOCALE		= es
NL_LOCALE		= nl
PT_LOCALE		= pt

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(CPU_ARCH),x86)
EMACS			= /bin/true
PLATFORM_FLAGS		+= -Di386
PORT_FLAGS		+= -DNEED_INET_TCP_H
else
PLATFORM_FLAGS		+= -D$(CPU_ARCH)
endif

ifeq ($(OS_VERSION),.3)
MOTIF			= /usr/local/Motif/opt/ICS/Motif/usr
MOTIFLIB		= $(MOTIF)/lib/libXm.a
EMACS			= /bin/true
endif
ifeq ($(OS_VERSION),.4)
PLATFORM_FLAGS		+= -DSOLARIS_24
endif
ifeq ($(OS_VERSION),.5)
PLATFORM_FLAGS		+= -DSOLARIS2_5 -DSOLARIS_55_OR_GREATER
endif
ifeq ($(OS_RELEASE)$(OS_VERSION),5.5.1)
PLATFORM_FLAGS		+= -DSOLARIS2_5 -DSOLARIS_55_OR_GREATER
RESOLV_LIB		= -lresolv
endif
ifeq ($(OS_VERSION),.6)
PLATFORM_FLAGS		+= -DSOLARIS2_6 -DSOLARIS_55_OR_GREATER -DSOLARIS_56_OR_GREATER
PORT_FLAGS		+= -DHAVE_SNPRINTF
RESOLV_LIB		= -lresolv
else
PORT_FLAGS		+= -DNEED_INET_TCP_H
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

BSDECHO			= /usr/ucb/echo
WHOAMI			= /usr/ucb/whoami
PROCESSOR_ARCHITECTURE	= _$(CPU_ARCH)
UNZIP_PROG		= $(NS_BIN)unzip
ZIP_PROG		= $(NS_BIN)zip

ifdef NETSCAPE_HIERARCHY
PERL			= perl5
endif

######################################################################
# Other
######################################################################

ifdef NS_USE_NATIVE
CC			= cc
CCC			= CC
NO_MDUPDATE		= 1
PORT_FLAGS		+= -DNS_USE_NATIVE
ASFLAGS			+= -Wa,-P
ifdef SERVER_BUILD
ifndef BUILD_OPT
PLATFORM_FLAGS		+= -xs
endif
endif
# -z gets around _sbrk multiple define.
OS_GPROF_FLAGS		= -xpg -z muldefs
DSO_CFLAGS		= -KPIC
else
PLATFORM_FLAGS		+= -Wall -Wno-format
OS_LIBS			+= -L$(NS_LIB)
ifneq ($(CPU_ARCH),x86)
ASFLAGS			+= -x assembler-with-cpp
else
ifndef BUILD_OPT
ifndef USE_AUTOCONF
OPTIMIZER		= -Wa,-s -gstabs
endif
endif
endif
OS_GPROF_FLAGS		= -pg
DSO_CFLAGS		= -fPIC
endif

ifndef NO_MDUPDATE
MDUPDATE_FLAGS		= -MDupdate $(DEPENDENCIES)
endif

ifeq ($(USE_PTHREADS),1)
ifdef NS_USE_NATIVE
CCC			+= -mt
endif
PORT_FLAGS		+= -D_REENTRANT
THREAD_LIB		= -lpthread
else
PORT_FLAGS		+= -DSW_THREADS
endif

#
# An nm command which generates an output like:
# archive.a:object.o: 0000003 T symbol
#
NM_PO			= nm -Ap

HAVE_PURIFY		= 1
MUST_BOOTLEG_ALLOCA	= 1
BUILD_UNIX_PLUGINS	= 1

# Turn on FULLCIRCLE crash reporting for 2.5.1 & up.
ifdef MOZ_FULLCIRCLE
FC_PLATFORM		= SolarisSparc
FC_PLATFORM_DIR		= SunOS5_sparc
endif

# use g++ to make shared libs
ifdef NS_USE_GCC
MKSHLIB			= $(CCC) $(DSO_LDOPTS)
DSO_LDOPTS		= -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so) -L$(MOTIF)/lib -L/usr/openwin/lib
else
MKSHLIB			= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS		= -G -L$(MOTIF)/lib -L/usr/openwin/lib
endif

DSO_BIND_REFERENCES	= -Bsymbolic

DSO_LDFLAGS		=
