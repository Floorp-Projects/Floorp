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
# Config stuff for Linux (all architectures)
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= linux
ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= x86
else
CPU_ARCH		:= $(OS_TEST)
endif
GFX_ARCH		:= x

OS_INCLUDES		=
G++INCLUDES		= -I/usr/include/g++
LOC_LIB_DIR		= /usr/lib/X11
MOTIF			=
MOTIFLIB		=
OS_LIBS			=

PLATFORM_FLAGS		= -ansi -Wall -pipe -DLINUX -Dlinux
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -D_POSIX_SOURCE -D_BSD_SOURCE -DSW_THREADS -DNEED_ENDIAN_H -DNEED_GETOPT_H -DNEED_IOCTL_H -DUSE_NODL_TABS -DHAVE_SIGNED_CHAR -DNEED_SYS_TIME_H -DHAVE_SYS_BITYPES_H -DNEED_UINT_T
PDJAVA_FLAGS		= -mx128m

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

LOCALE_MAP		= $(DEPTH)/cmd/xfe/intl/linux.lm
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

ifeq ($(CPU_ARCH),alpha)
PLATFORM_FLAGS		+= -DLINUX1_2 -D__$(CPU_ARCH) -D_ALPHA_ 
PORT_FLAGS		+= -DNEED_TIME_R -DMITSHM -D_XOPEN_SOURCE
OS_INCLUDES		+= -I/usr/X11R6/include
OS_LIBS			+= -L/lib -ldl -lc
endif
ifeq ($(CPU_ARCH),m68k)
PLATFORM_FLAGS		+= -m68020-40 -DLINUX1_2 -D$(CPU_ARCH)
PORT_FLAGS		+= -DNEED_TIME_R -DMITSHM -D_XOPEN_SOURCE
OS_INCLUDES		+= -I/usr/X11R6/include
OS_LIBS			+= -L/lib -ldl -lc
endif
ifeq ($(CPU_ARCH),ppc)
PLATFORM_FLAGS		+= -DMKLINUX -DLINUX1_2 -D$(CPU_ARCH)
OS_INCLUDES		+= -I/usr/local/include -I/usr/X11R6/include
endif
ifeq ($(CPU_ARCH),sparc)
PLATFORM_FLAGS		+= -DLINUX1_2 -D$(CPU_ARCH)
OS_INCLUDES		+= -I/usr/X11R6/include
endif
ifeq ($(CPU_ARCH),x86)
PLATFORM_FLAGS		+= -mno-486 -DLINUX1_2 -Di386
PORT_FLAGS		+= -DNEED_TIME_R -DMITSHM -D_XOPEN_SOURCE
OS_INCLUDES		+= -I/usr/X11R6/include
OS_LIBS			+= -L/lib -ldl -lc
endif

# These are CPU_ARCH independent
ifeq ($(OS_RELEASE),1.2)
PORT_FLAGS		+= -DNEED_SYS_WAIT_H
endif
ifeq ($(OS_RELEASE),2.0)
PORT_FLAGS		+= -DNO_INT64_T
PLATFORM_FLAGS		+= -DLINUX2_0
BUILD_UNIX_PLUGINS	= 1
MKSHLIB			= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
ifdef BUILD_OPT
OPTIMIZER		= -O2
endif
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

EMACS			= /bin/true
JAVA_PROG		= $(JAVA_BIN)java
PERL			= /usr/bin/perl
PROCESSOR_ARCHITECTURE	= _$(CPU_ARCH)
RANLIB			= /usr/bin/ranlib

ifneq ($(CPU_ARCH),ppc)
UNZIP_PROG		= /usr/bin/unzip
ZIP_PROG		= /usr/bin/zip
endif

######################################################################
# Other
######################################################################

ifeq ($(USE_PTHREADS),1)
PORT_FLAGS		+= -D_REENTRANT -D_PR_NEED_FAKE_POLL
else
PORT_FLAGS		+= -D_PR_LOCAL_THREADS_ONLY
endif

NEED_XMOS		= 1

DSO_CFLAGS		= -fpic
DSO_LDOPTS		= -shared
DSO_LDFLAGS		=

ifeq ($(USE_JDK11),1)
JAVA_HOME		= /usr/local/java
JAVAC_ZIP		= $(JAVA_HOME)/lib/classes.zip
endif 
