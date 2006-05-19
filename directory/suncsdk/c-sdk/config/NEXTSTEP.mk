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
# Config stuff for NEXTSTEP
#

include $(MOD_DEPTH)/config/UNIX.mk

CC			= cc
CCC			= cc++

RANLIB			= ranlib

OS_REL_CFLAGS		= -D$(shell uname -p)
CPU_ARCH		:= $(shell uname -p)

# "Commons" are tentative definitions in a global scope, like this:
#     int x;
# The meaning of a common is ambiguous.  It may be a true definition:
#     int x = 0;
# or it may be a declaration of a symbol defined in another file:
#     extern int x;
# Use the -fno-common option to force all commons to become true
# definitions so that the linker can catch multiply-defined symbols.
# Also, common symbols are not allowed with Rhapsody dynamic libraries.

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Wall -fno-common -pipe -DNEXTSTEP -DHAVE_STRERROR -DHAVE_BSD_FLOCK -D_POSIX_SOURCE -traditional-cpp -posix

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY

ARCH			= $(CPU_ARCH)

# May override this with -bundle to create a loadable module.
#DSO_LDOPTS		= -dynamiclib

#MKSHLIB			= $(CC) -arch $(CPU_ARCH) $(DSO_LDOPTS)
DLL_SUFFIX		= dylib
