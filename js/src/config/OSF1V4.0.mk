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
# Config stuff for Data General DG/UX
#

#
#  Initial DG/UX port by Marc Fraioli (fraioli@dg-rtp.dg.com)
#

ifdef NS_USE_GCC
CC = gcc
CCC = g++
CFLAGS +=  -mieee -Wall -Wno-format
else
CC  = cc
CCC = cxx
CFLAGS += -ieee -std
endif

RANLIB = echo
MKSHLIB = $(LD) -shared -all -expect_unresolved "*"

#
#  _DGUX_SOURCE is needed to turn on a lot of stuff in the headers if 
#      you're not using DG's compiler.  It shouldn't hurt if you are.
#
#  _POSIX4A_DRAFT10_SOURCE is needed to pick up localtime_r, used in
#      prtime.c
#
OS_CFLAGS = -DXP_UNIX -DSVR4 -DSYSV -DDGUX -D_DGUX_SOURCE -D_POSIX4A_DRAFT10_SOURCE -DOSF1
OS_LIBS = -lsocket -lnsl 

NOSUCHFILE = /no-such-file
