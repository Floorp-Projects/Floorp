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
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

#
# Config stuff for AIX
#

CC = xlC_r
CCC = xlC_r
CFLAGS += -qarch=com -qnoansialias -qinline+$(INLINES) -DXP_UNIX -DAIX -DAIXV3 -DSYSV

RANLIB = ranlib

#.c.o:
#	$(CC) -c -MD $*.d $(CFLAGS) $<
ARCH := aix
CPU_ARCH = rs6000
GFX_ARCH = x
INLINES = js_compare_and_swap:js_fast_lock1:js_fast_unlock1:js_lock_get_slot:js_lock_set_slot:js_lock_scope1

#-lpthreads -lc_r

MKSHLIB = /usr/lpp/xlC/bin/makeC++SharedLib_r -p 0 -G -berok

ifdef JS_THREADSAFE
XLDFLAGS += -ldl
endif

