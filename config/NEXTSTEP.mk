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
# Config stuff for NEXTSTEP
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= nextstep
CPU_ARCH		:= m68k
GFX_ARCH		:= x

OS_INCLUDES		=
G++INCLUDES		=
LOC_LIB_DIR		=
MOTIF			=
MOTIFLIB		=
OS_LIBS			=

PLATFORM_FLAGS		= -Wall -Wno-format -DNEXTSTEP -DRHAPSODY -D_NEXT_SOURCE
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSW_THREADS -DNO_CDEFS_H -DNO_REGEX -DNEED_BSDREGEX -DNO_REGCOMP -DHAS_PGNO_T -DNO_MULTICAST -DNO_TZNAME -D_POSIX_SOURCE -DNEED_S_ISLNK -DNEEDS_GETCWD -DNO_X11
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(OS_RELEASE),3.3)
PLATFORM_FLAGS		+= -DNEXTSTEP3 -DNEXTSTEP33
else
ifeq ($(OS_RELEASE),4.2)
PLATFORM_FLAGS		+= -DNEXTSTEP4 -DNEXTSTEP42
endif
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

CC			= cc
CCC			= c++
EMACS			= /bin/true
PERL			= /usr/bin/perl
AR              = /bin/libtool -static -o $@
RANLIB			= /bin/true
WHOAMI			= /usr/ucb/whoami

######################################################################
# Other
######################################################################

