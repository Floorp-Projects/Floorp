#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

#
# Config stuff for NetBSD
#

include $(DEPTH)/config/UNIX.mk

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		:= x86
else
CPU_ARCH		:= $(OS_TEST)
endif

#IMPL_STRATEGY		= _EMU
IMPL_STRATEGY		= 
DEFAULT_COMPILER		= gcc
CC				= gcc
CXX				= g++
AS				= gcc -c
RANLIB			= ranlib
MKSHLIB			= $(CC) $(DSO_LDOPTS)
MKMODULE		= ld -Ur -o $@

WARNING_CFLAG	= -Wall

# used by mkdepend
X11INCLUDES		=   -I/usr/X11R6/include
SYS_INCLUDES		=   -I$(subst libgcc.a,include, \
                                      $(shell $(CC) -print-libgcc-file-name))
SYS_INCLUDES		+=  -I$(subst libgcc.a,include, \
                                      $(shell $(CCC) -print-libgcc-file-name))

ifeq ($(CPU_ARCH),x86)
DEPENDFLAGS		+= -D__i386__
endif

OS_REL_CFLAGS           = -mno-486 -Di386

OS_CFLAGS               = -DGLOBALS_NEED_UNDERSCORE

OS_CFLAGS		+= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -DNETBSD -ansi -Wall -pipe -DHAVE_STRERROR -DHAVE_BSD_FLOCK
OS_CXXFLAGS		= $(OS_CFLAGS)
OS_ASFLAGS		= -DNETBSD
OS_LDFLAGS		=
OS_LIBS			= -lm

DSO_CFLAGS		= -fPIC
DSO_LDFLAGS		= -Wl,export-dynamic
DSO_LDOPTS		= -shared 

PERL			= perl
