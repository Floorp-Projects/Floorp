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
# Config stuff for BSD/OS Unix.
#

include $(MOD_DEPTH)/config/UNIX.mk

ifeq (,$(filter-out 1.1 4.%,$(OS_RELEASE)))
CC		= gcc -Wall -Wno-format
CCC		= g++
else
CC		= shlicc2
CCC		= shlicc2
endif
RANLIB		= ranlib

ifeq ($(USE_PTHREADS),1)
IMPL_STRATEGY = _PTH
DEFINES		+= -D_PR_NEED_PTHREAD_INIT
else
IMPL_STRATEGY = _EMU
DEFINES		+= -D_PR_LOCAL_THREADS_ONLY
endif

OS_CFLAGS	= $(DSO_CFLAGS) -DBSDI -DHAVE_STRERROR -DNEED_BSDREGEX

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH	= x86
endif
ifeq (sparc,$(findstring sparc,$(OS_TEST)))
CPU_ARCH	= sparc
endif

ifeq ($(OS_RELEASE),2.1)
OS_CFLAGS	+= -D_PR_TIMESPEC_HAS_TS_SEC
endif

ifeq (,$(filter-out 1.1 2.1,$(OS_RELEASE)))
OS_CFLAGS	+= -D_PR_BSDI_JMPBUF_IS_ARRAY
else
OS_CFLAGS	+= -D_PR_SELECT_CONST_TIMEVAL -D_PR_BSDI_JMPBUF_IS_STRUCT
endif

NOSUCHFILE	= /no-such-file

ifeq ($(OS_RELEASE),1.1)
OS_CFLAGS	+= -D_PR_STAT_HAS_ONLY_ST_ATIME -D_PR_NEED_H_ERRNO
else
OS_CFLAGS	+= -DHAVE_DLL -DUSE_DLFCN -D_PR_STAT_HAS_ST_ATIMESPEC
OS_LIBS		= -ldl
ifeq (,$(filter-out 4.%,$(OS_RELEASE)))
MKSHLIB		= $(CC) $(DSO_LDOPTS)
DSO_CFLAGS	= -fPIC
DSO_LDOPTS	= -shared -Wl,-soname,$(@:$(OBJDIR)/%.so=%.so)
else
MKSHLIB		= $(LD) $(DSO_LDOPTS)
DSO_LDOPTS	= -r
endif
endif
