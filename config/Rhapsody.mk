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
# Config stuff for Rhapsody
######################################################################
#
######################################################################
# Version-independent
######################################################################

ARCH			:= rhapsody
ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= i386
else
CPU_ARCH		:= ppc
endif
GFX_ARCH		:=

OS_INCLUDES		= 
G++INCLUDES		= -I/usr/include/g++
LOC_LIB_DIR		= 
MOTIF			=
MOTIFLIB		=
OS_LIBS			= -lstdc++ 

PLATFORM_FLAGS		= -Wall -pipe -DRHAPSODY -D$(CPU_ARCH)
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSW_THREADS -DHAVE_STDDEF_H -DHAVE_STDLIB_H -DHAVE_FILIO_H -DNTOHL_ENDIAN_H -DMACHINE_ENDIAN_H -DNO_REGEX -DNO_REGCOMP -DHAS_PGNO_T -DNO_TZNAME -DNEEDS_GETCWD -DHAVE_SYSERRLIST
PDJAVA_FLAGS		=

# "Commons" are tentative definitions in a global scope, like this:
#     int x;
# The meaning of a common is ambiguous.  It may be a true definition:
#     int x = 0;
# or it may be a declaration of a symbol defined in another file:
#     extern int x;
# Use the -fno-common option to force all commons to become true
# definitions so that the linker can catch multiply-defined symbols.
# Also, common symbols are not allowed with Rhapsody dynamic libraries.
DSO_FLAGS		= -fno-common

# Build Mozilla/X11 version, else build NGLayout/ybfe.
ifdef HAVE_X11
OS_INCLUDES 		+= -I/usr/X11R6/include
OS_LIBS 		+= -L/usr/X11R6/lib -lXm -lXt -lXext -lX11
PORT_FLAGS		+= 
else
PORT_FLAGS		+= -DNO_X11
NO_X11			= 1
endif

ifdef USE_AUTOCONF
OS_CFLAGS		= $(DSO_FLAGS)
else
OS_CFLAGS		= $(PLATFORM_FLAGS) $(DSO_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)
endif

######################################################################
# Version-specific stuff
######################################################################

ifeq ($(CPU_ARCH),i386)
PLATFORM_FLAGS		+= -mno-486
endif

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

# 5.1 renamed cc++ to c++.
ifeq ($(OS_RELEASE),5.0)
CCC			= cc++
else
CCC			= c++
endif

CC			= cc
AR			= libtool -static -o $@

EMACS			= /usr/bin/emacs
PERL			= /usr/bin/perl
RANLIB			= ranlib

LDFLAGS			= 

# -nostdlib gets around the missing -lm problem.
DSO_LDFLAGS		= -arch $(CPU_ARCH) -dynamiclib -nostdlib -lstdc++ -lcc_dynamic -compatibility_version 1 -current_version 1 -all_load -undefined suppress

# Comment out MKSHLIB to build only static libraries.
MKSHLIB			= $(CC) $(DSO_LDFLAGS)
DLL_SUFFIX		= dylib

######################################################################
# Other
######################################################################

