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
# Config stuff for AIX.
#

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
CLASSIC_NSPR = 1
endif
endif

#
# There are three implementation strategies available on AIX:
# pthreads, classic, and pthreads-user.
#
# On AIX 3.2, classic nspr is the default (and only) implementation
# strategy.  On AIX 4.1 and later, the default is pthreads.
# 
ifeq ($(OS_RELEASE),3.2)
CLASSIC_NSPR = 1
endif

ifeq ($(CLASSIC_NSPR),1)
	PTHREADS_USER =
	USE_PTHREADS =
	IMPL_STRATEGY = _EMU
	DEFINES += -D_PR_LOCAL_THREADS_ONLY
else
ifeq ($(PTHREADS_USER),1)
	USE_PTHREADS =
	IMPL_STRATEGY = _PTH_USER
else
	USE_PTHREADS = 1
ifeq ($(HAVE_CCONF), 1)
	IMPL_STRATEGY =
else
	IMPL_STRATEGY = _PTH
endif
endif
endif

# IPv6 support part of the standard AIX 4.3 release.
ifneq (,$(filter-out 3.2 4.1 4.2,$(OS_RELEASE)))
USE_IPV6 = 1
endif

ifeq ($(CLASSIC_NSPR),1)
CC		= xlC
CCC		= xlC
else
CC		= xlC_r
CCC		= xlC_r
endif
OS_CFLAGS	= -qro -qroconst
ifeq ($(USE_64),1)
OBJECT_MODE	= 64
export OBJECT_MODE
COMPILER_TAG	= _64
else
ifeq ($(HAVE_CCONF), 1)
COMPILER_TAG	=
else
COMPILER_TAG	= _32
endif
endif

CPU_ARCH	= rs6000

RANLIB		= ranlib

OS_CFLAGS 	+= -DAIX -DSYSV
ifeq ($(CC),xlC_r)
OS_CFLAGS 	+= -qarch=com
endif

ifneq ($(OS_RELEASE),3.2)
OS_CFLAGS	+= -DAIX_HAVE_ATOMIC_OP_H -DAIX_TIMERS
endif

ifeq (,$(filter-out 3.2 4.1,$(OS_RELEASE)))
ifndef USE_PTHREADS
OS_CFLAGS	+= -DAIX_RENAME_SELECT
endif
endif

ifeq (,$(filter-out 3.2 4.1,$(OS_RELEASE)))
OS_CFLAGS	+= -D_PR_NO_LARGE_FILES
else
OS_CFLAGS	+= -D_PR_HAVE_OFF64_T -D_LARGEFILE64_SOURCE
endif

ifeq ($(OS_RELEASE),4.1)
OS_CFLAGS	+= -DAIX4_1
else
DSO_LDOPTS	= -brtl -bM:SRE -bnoentry -bexpall
MKSHLIB		= $(LD) $(DSO_LDOPTS)
ifeq ($(OS_RELEASE),4.3)
OS_CFLAGS	+= -DAIX4_3
endif
endif

# Have the socklen_t data type
ifeq ($(OS_RELEASE),4.3)
OS_CFLAGS	+= -DHAVE_SOCKLEN_T
endif

ifeq (,$(filter-out 4.2 4.3,$(OS_RELEASE)))
# On these OS revisions, localtime_r() is declared if _THREAD_SAFE
# is defined.
ifneq ($(CLASSIC_NSPR),1)
OS_CFLAGS	+= -DHAVE_POINTER_LOCALTIME_R
endif
endif

ifeq (,$(filter-out 4.3,$(OS_RELEASE)))
# On these OS revisions, gethostbyXXX() returns result in thread
# specific storage.
ifeq ($(USE_PTHREADS),1)
OS_CFLAGS	+= -D_PR_HAVE_THREADSAFE_GETHOST
endif
endif

#
# Special link info for constructing AIX programs. On AIX we have to
# statically link programs that use NSPR into a single .o, rewriting the
# calls to select to call "aix". Once that is done we then can
# link that .o with a .o built in nspr which implements the system call.
#
ifneq (,$(filter-out 3.2 4.1,$(OS_RELEASE)))
AIX_LINK_OPTS	= -brtl -bnso -berok
else
AIX_LINK_OPTS	= -bnso -berok
#AIX_LINK_OPTS	= -bnso -berok -brename:.select,.wrap_select -brename:.poll,.wrap_poll -bI:/usr/lib/syscalls.exp
endif

AIX_WRAP	= $(DIST)/lib/aixwrap.o
AIX_TMP		= $(OBJDIR)/_aix_tmp.o
