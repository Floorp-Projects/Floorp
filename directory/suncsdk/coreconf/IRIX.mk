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
# Config stuff for IRIX
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# The default implementation strategy for Irix is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _n32_PTH
endif

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC		= gcc
	AS		= $(CC) -x assembler-with-cpp
	ODD_CFLAGS	= -Wall -Wno-format
	ifdef BUILD_OPT
		OPTIMIZER	= -O6
	endif
else
	CC	= cc
	CCC		= CC
	ODD_CFLAGS	= -fullwarn -xansi
	ifdef BUILD_OPT
		ifeq ($(USE_N32),1)
			OPTIMIZER	= -O -OPT:Olimit=4000
		else
			OPTIMIZER	= -O -Olimit 4000
		endif
	endif

	# For 6.x machines, include this flag
	ifeq (6., $(findstring 6., $(OS_RELEASE)))
		ifeq ($(USE_N32),1)
			ODD_CFLAGS	+= -n32 -exceptions
		else
			ODD_CFLAGS	+= -32 -multigot
		endif
	else
		ODD_CFLAGS		+= -xgot
	endif
endif

ODD_CFLAGS	+= -DSVR4 -DIRIX 

CPU_ARCH	= mips

RANLIB		= /bin/true
# For purify
# XXX: should always define _SGI_MP_SOURCE
NOMD_OS_CFLAGS += $(ODD_CFLAGS) -D_SGI_MP_SOURCE

ifndef NO_MDUPDATE
	OS_CFLAGS += $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
else
	OS_CFLAGS += $(NOMD_OS_CFLAGS)
endif

ifeq ($(USE_N32),1)
	SHLIB_LD_OPTS	+= -n32
endif

MKSHLIB     += $(LD) $(SHLIB_LD_OPTS) -shared -soname $(@:$(OBJDIR)/%.so=%.so)

HAVE_PURIFY	= 1

DSO_LDOPTS	= -elf -shared -all

ifdef DSO_BACKEND
	DSO_LDOPTS += -soname $(DSO_NAME)
endif
