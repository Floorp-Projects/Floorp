# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#
# Config stuff for OpenBSD
#

include $(MOD_DEPTH)/config/UNIX.mk

CC                     = gcc
CCC                    = g++
RANLIB                 = ranlib

OS_REL_CFLAGS          =
ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH               = x86
else
CPU_ARCH               = $(OS_TEST)
endif

OS_CFLAGS              = $(DSO_CFLAGS) $(OS_REL_CFLAGS) -ansi -Wall -pipe $(THREAD_FLAG) -DOPENBSD -DHAVE_STRERROR -DHAVE_BSD_FLOCK

ifeq ($(USE_PTHREADS),1)
THREAD_FLAG		+= -pthread
# XXX probably should define _THREAD_SAFE too.
else
DEFINES                        += -D_PR_LOCAL_THREADS_ONLY
endif

ARCH                   = openbsd

DLL_SUFFIX             = so.1.0

DSO_CFLAGS             = -fPIC
DSO_LDOPTS             = -Bshareable
ifeq ($(OS_TEST),alpha)
DSO_LDOPTS             = -shared
endif
ifeq ($(OS_TEST),mips)
DSO_LDOPTS             = -shared
endif
ifeq ($(OS_TEST),pmax)  
DSO_LDOPTS             = -shared
endif

MKSHLIB                        = $(LD) $(DSO_LDOPTS)

G++INCLUDES            = -I/usr/include/g++
