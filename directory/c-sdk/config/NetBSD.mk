# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

#
# Config stuff for NetBSD
#

include $(MOD_DEPTH)/config/UNIX.mk

CC			= gcc
CCC			= g++
RANLIB			= ranlib

ifndef OBJECT_FMT
OBJECT_FMT		:= $(shell if echo __ELF__ | $${CC:-cc} -E - | grep -q __ELF__ ; then echo a.out ; else echo ELF ; fi)
endif

OS_REL_CFLAGS		=
ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		= x86
else
CPU_ARCH		= $(OS_TEST)
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe -DNETBSD -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ifeq ($(USE_PTHREADS),1)
OS_LIBS			= -lc_r
# XXX probably should define _THREAD_SAFE too.
else
OS_LIBS			= -lc
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
endif

ARCH			= netbsd

ifeq ($(OBJECT_FMT),ELF)
DLL_SUFFIX		= so
else
DLL_SUFFIX		= so.1.0
# XXX work around a bug in the a.out ld(1).
OS_LIBS			=
endif

DSO_CFLAGS		= -fPIC -DPIC
DSO_LDOPTS		= -shared
ifeq ($(OBJECT_FMT),ELF)
DSO_LDOPTS		+=-Wl,-soname,lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
endif

ifdef LIBRUNPATH
DSO_LDOPTS		+= -Wl,-R$(LIBRUNPATH)
endif

MKSHLIB			= $(CC) $(DSO_LDOPTS)

G++INCLUDES		= -I/usr/include/g++
