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
# Config stuff for QNX.
######################################################################

######################################################################
# Version-independent
######################################################################

ARCH			:= qnx
CPU_ARCH		:= x86
GFX_ARCH		:= x

ifndef NS_USE_GCC
CC			= cc
CCC			= cc
endif
BSDECHO			= echo
EMACS			= true
RANLIB			= true

OS_INCLUDES		= -I/usr/X11/include
G++INCLUDES		=
LOC_LIB_DIR		= 
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			=

PLATFORM_FLAGS		= -DQNX -Di386
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSW_THREADS -DHAVE_STDDEF_H -DHAVE_STDLIB_H -DNO_CDEFS_H -DNO_LONG_LONG -DNO_ALLOCA -DNO_REGEX -DNO_REGCOMP -DHAS_PGNO_T -DNEED_REALPATH
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

######################################################################
# Other
######################################################################

ifdef SERVER_BUILD
STATIC_JAVA		= yes
endif
