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
# Config stuff for SCO OpenServer for x86.
# SCO OpenServer 5, based on SVR3.2, is intended for small to
# medium customers.
#

include $(MOD_DEPTH)/config/UNIX.mk

CC			= cc -b elf -KPIC
CCC			= $(NSDEPTH)/build/hcpp CC +.cpp +w
RANLIB			= /bin/true

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
#
# -DSCO - Changes to Netscape source (consistent with AIX, LINUX, etc..)
# -Dsco - Needed for /usr/include/X11/*
#
OS_CFLAGS		= -DSYSV -D_SVID3 -DHAVE_STRERROR -D_PR_NEED_H_ERRNO -DSCO -Dsco
#OS_LIBS			= -lpmapi -lsocket -lc

MKSHLIB			= $(LD) $(DSO_LDOPTS)

CPU_ARCH		= x86
ARCH			= sco

NOSUCHFILE		= /no-such-file

BSDECHO			= /bin/echo

DSO_LDOPTS		= -b elf -G
