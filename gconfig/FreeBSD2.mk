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
# Config stuff for FreeBSD2
#

include $(GDEPTH)/gconfig/UNIX.mk

DEFAULT_COMPILER	= gcc
CC			= gcc
CCC			= g++
RANLIB			= ranlib

OS_REL_CFLAGS		= -mno-486 -Di386
CPU_ARCH		= x86

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DFREEBSD -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ifdef USE_PTHREADS
	OS_LIBS		+= -lc_r
	DEFINES		+= -D_PR_NEED_FAKE_POLL
else
	OS_LIBS		+= -lc
endif

ARCH			= freebsd

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -Bshareable
DSO_LDFLAGS		=

MKSHLIB			= $(LD) $(DSO_LDOPTS)

G++INCLUDES		= -I/usr/include/g++

INCLUDES		+= -I/usr/X11R6/include
