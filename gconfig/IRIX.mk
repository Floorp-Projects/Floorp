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
# Config stuff for IRIX
#

include $(GDEPTH)/gconfig/UNIX.mk

#
# The default implementation strategy for Irix is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

DEFAULT_COMPILER = cc
LINK_PROGRAM = $(CCC)
LD = $(CCC)

ifdef NS_USE_GCC
	CC		= gcc
	AS		= $(CC) -x assembler-with-cpp
	ODD_CFLAGS	= -Wall -Wno-format
	ifdef BUILD_OPT
		OPTIMIZER	= -O6
	endif
else
	ifeq ($(USE_N32),1)
		CC	= CC
	else
		CC	= cc
	endif
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
			ODD_CFLAGS	+= -n32 -exceptions -woff 1209,1642,3201 -DIRIX_N32 -DXP_CPLUSPLUS
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

AR_ALL  = -all
AR_NONE = -none