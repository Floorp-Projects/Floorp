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
# Config stuff for NEC Mips SYSV
#

include $(GDEPTH)/gconfig/UNIX.mk

DEFAULT_COMPILER = $(GDEPTH)/build/hcc

CPU_ARCH		= mips

ifdef NS_USE_GCC
CC			= gcc
CCC			= g++
else
CC			= $(GDEPTH)/build/hcc
OS_CFLAGS		= -Xa -KGnum=0 -KOlimit=4000
CCC			= g++
endif

MKSHLIB			= $(LD) $(DSO_LDOPTS)

RANLIB			= /bin/true

OS_CFLAGS		+= $(ODD_CFLAGS) -DSVR4 -D__SVR4 -DNEC -Dnec_ews -DHAVE_STRERROR
OS_LIBS			+= -lsocket -lnsl -ldl $(LDOPTIONS)
LDOPTIONS		= -lc -L/usr/ucblib -lucb

NOSUCHFILE		= /nec-rm-f-sucks

DSO_LDOPTS		= -G
