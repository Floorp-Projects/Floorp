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
# Config stuff for IRIX
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
# On IRIX 5.x, classic nspr (user-level threads on top of sprocs)
# is the default (and only) implementation strategy.
#
# On IRIX 6.x and later, the default implementation strategy is
# pthreads.  Classic nspr is also available.
#
ifeq ($(basename $(OS_RELEASE)),5)
CLASSIC_NSPR = 1
endif

ifeq ($(CLASSIC_NSPR),1)
	IMPL_STRATEGY = _MxN
else
	USE_PTHREADS = 1
	USE_N32 = 1
	IMPL_STRATEGY = _PTH
endif

ifeq ($(NS_USE_GCC), 1) 
	CC			= gcc
	COMPILER_TAG		= _gcc
	AS			= $(CC) -x assembler-with-cpp -D_ASM -mips2
	ODD_CFLAGS		= -Wall -Wno-format
	ifdef BUILD_OPT
		OPTIMIZER		= -O6
	endif
else
	CC			= cc
	CCC         = CC
	ODD_CFLAGS		= -fullwarn -xansi
	ifdef BUILD_OPT
		ifneq ($(USE_N32),1)
			OPTIMIZER		= -O -Olimit 4000
		else
			OPTIMIZER		= -O -OPT:Olimit=4000
		endif
	endif

#
# The default behavior is still -o32 generation, hence the explicit tests
# for -n32 and -64 and implicitly assuming -o32. If that changes, ...
#
	ifeq ($(basename $(OS_RELEASE)),6)
		ODD_CFLAGS += -multigot
		SHLIB_LD_OPTS = -no_unresolved
		ifeq ($(USE_N32),1)
			ODD_CFLAGS += -n32 -woff 1209
			COMPILER_TAG = _n32
			LDOPTS += -n32
        	SHLIB_LD_OPTS += -n32
			ifeq ($(OS_RELEASE), 6_2)
				LDOPTS += -Wl,-woff,85
				SHLIB_LD_OPTS += -woff 85
			endif
		else
			ifeq ($(USE_64),1)
				ODD_CFLAGS +=  -64
				COMPILER_TAG = _64
			else
				ODD_CFLAGS +=  -32
				COMPILER_TAG = _o32
			endif
		endif
	else
		ODD_CFLAGS += -xgot
	endif
endif

ODD_CFLAGS		+= -DSVR4 -DIRIX

CPU_ARCH		= mips

RANLIB			= /bin/true

# For purify
# XXX: should always define _SGI_MP_SOURCE
NOMD_OS_CFLAGS		= $(ODD_CFLAGS) -D_SGI_MP_SOURCE

ifeq ($(OS_RELEASE),5.3)
OS_CFLAGS		+= -DIRIX5_3
endif

ifneq ($(basename $(OS_RELEASE)),5)
OS_CFLAGS		+= -D_PR_HAVE_SGI_PRDA_PROCMASK
endif

ifeq (,$(filter-out 6.5,$(OS_RELEASE)))
ifneq ($(NS_USE_GCC), 1)
OS_CFLAGS		+= -mips3
endif
OS_CFLAGS		+= -D_PR_HAVE_GETPROTO_R -D_PR_HAVE_GETPROTO_R_POINTER
ifeq ($(USE_PTHREADS),1)
OS_CFLAGS		+= -D_PR_HAVE_GETHOST_R -D_PR_HAVE_GETHOST_R_POINTER
endif
endif

ifndef NO_MDUPDATE
OS_CFLAGS		+= $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
else
OS_CFLAGS		+= $(NOMD_OS_CFLAGS)
endif

# -rdata_shared is an ld option that puts string constants and
# const data into the text segment, where they will be shared
# across processes and be read-only.
MKSHLIB			= $(LD) $(SHLIB_LD_OPTS) -rdata_shared -shared -soname $(notdir $@)

DSO_LDOPTS		= -elf -shared -all
