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
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

#
# Config stuff for AIX.
#
include $(GDEPTH)/gconfig/UNIX.mk

#
# XXX
# There are two implementation strategies available on AIX:
# pthreads, and pthreads-user.  The default is pthreads.
# In both strategies, we need to use pthread_user.c, instead of
# aix.c.  The fact that aix.c is never used is somewhat strange.
# 
# So we need to do the following:
# - Default (PTHREADS_USER not defined in the environment or on
#   the command line):
#   Set PTHREADS_USER=1, USE_PTHREADS=1
# - PTHREADS_USER=1 set in the environment or on the command line:
#   Do nothing.
#
ifeq ($(PTHREADS_USER),1)
	USE_PTHREADS =            # just to be safe
	IMPL_STRATEGY = _PTH_USER
else
	USE_PTHREADS = 1
	PTHREADS_USER = 1
endif

DEFAULT_COMPILER = xlC_r

CC		= xlC_r
CCC		= xlC_r

CPU_ARCH	= rs6000

RANLIB		= ranlib

OS_CFLAGS	= -DAIX -DSYSV
ifeq ($(CC),xlC_r)
	OS_CFLAGS += -qarch=com
endif

AIX_WRAP	= $(DIST)/lib/aixwrap.o
AIX_TMP		= $(OBJDIR)/_aix_tmp.o
