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
# Config stuff for NCR SVR4 MP-RAS
#

include $(MOD_DEPTH)/config/UNIX.mk
###
NS_USE_NATIVE = 1

# NS_USE_GCC = 1

export PATH:=$(PATH):/opt/ncc/bin
###

RANLIB          = true
GCC_FLAGS_EXTRA = -pipe

DEFINES		+= -DSVR4 -DSYSV -DHAVE_STRERROR -DNCR -D_PR_LOCAL_THREADS_ONLY

ifeq (,$(filter-out 2.03,$(OS_RELEASE)))
DEFINES		+= -D_PR_STAT_HAS_ST_ATIM
else
DEFINES		+= -D_PR_STAT_HAS_ST_ATIM_UNION
endif

ifdef NS_USE_NATIVE
CC              = cc
CCC             = ncc
OS_CFLAGS	= -Hnocopyr
#OS_LIBS         = -L/opt/ncc/lib 
else
#OS_LIBS		=
endif

CCC	= g++

#OS_LIBS	       += -lsocket -lnsl -ldl -lc

MKSHLIB		= $(LD) $(DSO_LDOPTS)
#DSO_LDOPTS	= -G -z defs
DSO_LDOPTS	= -G

CPU_ARCH	= x86
ARCH		= ncr

NOSUCHFILE	= /no-such-file

# now take care of default GCC (rus@5/5/97)
 
ifdef NS_USE_GCC
# if gcc-settings are redefined already - don't touch it
#
ifeq (,$(findstring gcc, $(CC)))
CC      = gcc
CCC     = g++
CXX     = g++
COMPILER_TAG = _gcc
# always use -fPIC - some makefiles are still broken and don't distinguish
# situation when they build shared and static libraries
CFLAGS  += -fPIC -Wall $(GCC_FLAGS_EXTRA)
#OS_LIBS += -L/usr/local/lib -lstdc++ -lg++ -lgcc
endif
endif
###

