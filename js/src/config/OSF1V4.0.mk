# -*- Mode: makefile -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#

#
# Config stuff for Data General DG/UX
#

#
#  Initial DG/UX port by Marc Fraioli (fraioli@dg-rtp.dg.com)
#

ifndef NS_USE_NATIVE
CC = gcc
CCC = g++
CFLAGS +=  -mieee -Wall -Wno-format
else
CC  = cc
CCC = cxx
CFLAGS += -ieee -std
# LD  = cxx
endif

RANLIB = echo
MKSHLIB = $(LD) -shared -taso -all -expect_unresolved "*"

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
