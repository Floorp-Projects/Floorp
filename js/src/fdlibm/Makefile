#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Sun Microsystems,
# Inc.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

#
#  @(#)Makefile 1.4 95/01/18 
# 
#  ====================================================
#  Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
# 
#  Developed at SunSoft, a Sun Microsystems, Inc. business.
#  Permission to use, copy, modify, and distribute this
#  software is freely granted, provided that this notice 
#  is preserved.
#  ====================================================
# 
# 

DEPTH           = ../../..

include $(DEPTH)/config/config.mk

#
# Default IEEE libm
#
CFLAGS          += -D_IEEE_LIBM

LIBRARY_NAME	= fdm

MODULE		= js

CSRCS		=	\
	e_acos.c	\
	e_asin.c	\
	e_atan2.c	\
	e_exp.c		\
	e_fmod.c	\
	e_log.c		\
	e_pow.c		\
	e_rem_pio2.c	\
	s_scalbn.c	\
	e_sqrt.c 	\
	k_cos.c		\
	k_sin.c		\
	k_rem_pio2.c	\
	k_tan.c 	\
	s_atan.c	\
	s_ceil.c	\
	s_copysign.c	\
	s_cos.c		\
	s_fabs.c	\
	s_finite.c	\
	s_floor.c	\
	s_isnan.c	\
	s_lib_version.c	\
	s_sin.c		\
	s_tan.c		\
	w_acos.c	\
	w_asin.c	\
	w_atan2.c	\
	w_exp.c		\
	w_fmod.c	\
	w_log.c		\
	w_pow.c		\
	w_sqrt.c	\
	$(NULL)

EXPORTS	= fdlibm.h

include $(DEPTH)/config/rules.mk

# from mozilla/js/src/Makefile
ifeq ($(CPU_ARCH),sparc)

ifndef JS_NO_ULTRA
ULTRA_OPTIONS := -xarch=v8plus,-DULTRA_SPARC
ULTRA_OPTIONSCC := -DULTRA_SPARC
else
ULTRA_OPTIONS := -xarch=v8
ULTRA_OPTIONSCC :=
endif

ifeq ($(shell uname -m),sun4u)
ASFLAGS	+= -Wa,$(ULTRA_OPTIONS),-P,-L,-D_ASM,-D__STDC__=0 $(ULTRA_OPTIONSCC)
else
ASFLAGS	+= -Wa,-xarch=v8,-P,-L,-D_ASM,-D__STDC__=0
endif

endif # sparc

ifeq ($(OS_ARCH), OSF1)
LDFLAGS += -lc_r
endif
ifeq ($(OS_ARCH), SunOS)
LDFLAGS += -lposix4 -ldl -lnsl -lsocket
endif
ifeq ($(OS_ARCH), Linux)
LDFLAGS += -ldl
endif
# end from mozilla/js/src/Makefile
