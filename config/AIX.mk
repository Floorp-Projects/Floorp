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
# Config stuff for IBM AIX for RS/6000 and PPC
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= aix
CPU_ARCH		:= rs6000  # How can I determine this?
GFX_ARCH		:= x

OS_INCLUDES		=
G++INCLUDES		=
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			= -lm -lbsd

PLATFORM_FLAGS		= -qarch=com -qmaxmem=65536 -DAIX -Daix
MOVEMAIL_FLAGS		=
PORT_FLAGS		= -DSYSV -DNEED_CDEFS_H -DNEED_SELECT_H -DNEED_IOCTL_H -DSYS_MACHINE_H -DUSE_NODL_TABS -DHAVE_SIGNED_CHAR -DHAVE_SYS_SELECT_H -DNEED_SYS_WAIT_H -DHAVE_INT32_T -DNEED_H_ERRNO
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/aix.lm
EN_LOCALE		= en_US.ISO8859-1
DE_LOCALE		= de_DE.ISO8859-1
FR_LOCALE		= fr_FR.ISO8859-1
JP_LOCALE		= ja_JP.IBM-eucJP
SJIS_LOCALE		= Ja_JP.IBM-932
KR_LOCALE		= ko_KR.IBM-eucKR
CN_LOCALE		= zh_CN
TW_LOCALE		= zh_TW.IBM-eucTW
I2_LOCALE		= iso88592

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(OS_RELEASE),3.2)
PLATFORM_FLAGS		+= -qtune=601 -DAIXV3 -DAIX3_2_5
PORT_FLAGS		+= -DSW_THREADS
else
PLATFORM_FLAGS		+= -qtune=604 -qnosom -DAIXV4
endif
ifeq ($(OS_RELEASE),4.1)
PLATFORM_FLAGS		+= -DAIX4_1
PORT_FLAGS		+= -DSW_THREADS
OS_LIBS			+= -lsvld 
DSO_LDOPTS		= -bM:SRE -bh:4 -bnoentry
AIX_NSPR		= $(DIST)/bin/libnspr_shr.a
LIBNSPR			= $(AIX_NSPR)
AIX_NSPR_LINK		= -L$(DIST)/bin -lnspr_shr -blibpath:/usr/local/lib/netscape:/usr/lib:/lib:.
#
# Used to link java, javah.  Include 3 relative paths since we're guessing
# at runtime where the hell the library is.  LIBPATH can be set, but
# setting this will be hell for release people, _AND_ I couldn't get it to 
# work.  Sigh. -mcafee
#
AIX_NSPR_DIST_LINK	= -L$(DIST)/bin -lnspr_shr -blibpath:.:../dist/$(OBJDIR)/bin:../../dist/$(OBJDIR)/bin:../../../dist/$(OBJDIR)/bin:/usr/lib:/lib
endif
ifneq (,$(filter 4.2 4.3,$(OS_RELEASE)))
PORT_FLAGS		+= -DHW_THREADS -DUSE_PTHREADS -DPOSIX7
OS_LIBS			+= -ldl 
MKSHLIB			= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS		= -brtl -bM:SRE -bnoentry -bexpall
ifeq ($(OS_RELEASE),4.2)
PLATFORM_FLAGS		+= -DAIX4_2
endif
ifeq ($(OS_RELEASE),4.3)
PLATFORM_FLAGS		+= -DAIX4_3
endif
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

CC			= cc
CCC			= xlC -+
BSDECHO			= $(DIST)/bin/bsdecho
RANLIB			= /usr/ccs/bin/ranlib
WHOAMI			= /bin/whoami

ifneq ($(OS_RELEASE),3.2)
UNZIP_PROG		= $(CONTRIB_BIN)unzip
ZIP_PROG		= $(CONTRIB_BIN)zip
endif

######################################################################
# Other
######################################################################

ifdef SERVER_BUILD
CC			= xlC_r
# In order to automatically generate export lists, we need to use -g with -O
OPTIMIZER		= -g -O
PORT_FLAGS		+= -DFORCE_PR_LOG -D_PR_PTHREADS -UHAVE_CVAR_BUILT_ON_SEM -DFD_SETSIZE=4096
endif

#ifeq ($(PTHREADS_USER),1)
#USE_PTHREADS		=
#else
#USE_PTHREADS		= 1
#endif

MUST_BOOTLEG_ALLOCA	= 1
BUILD_UNIX_PLUGINS	= 1 

DSO_LDFLAGS		= -lXm -lXt -lX11
EXTRA_DSO_LDOPTS	= -lc
