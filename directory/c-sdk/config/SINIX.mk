# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

#
# Config stuff for SNI SINIX (aka ReliantUNIX)
#

include $(MOD_DEPTH)/config/UNIX.mk

ifeq (86,$(findstring 86,$(OS_TEST)))
CPU_ARCH		= x86
else
CPU_ARCH		= mips
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

# use gcc -tf-
NS_USE_GCC		= 1

ifeq ($(NS_USE_GCC),1)
## gcc-2.7.2 homebrewn
CC			= gcc
COMPILER_TAG		= _gcc
CCC			= g++
AS			= $(CC) -x assembler-with-cpp
ifeq ($(CPU_ARCH),mips)
LD			= gld
endif
ODD_CFLAGS		= -Wall -Wno-format
ifeq ($(CPU_ARCH),mips)
# The -pipe flag only seems to work on the mips version of SINIX.
ODD_CFLAGS		+= -pipe
endif
ifdef BUILD_OPT
OPTIMIZER		= -O
#OPTIMIZER		= -O6
endif
MKSHLIB			= $(LD) -G -z defs -h $(@:$(OBJDIR)/%.so=%.so)
#DSO_LDOPTS		= -G -Xlinker -Blargedynsym
else
## native compiler (CDS++ 1.0)
CC			= /usr/bin/cc
CCC			= /usr/bin/CC
AS			= /usr/bin/cc
#ODD_CFLAGS		= -fullwarn -xansi
ODD_CFLAGS		= 
ifdef BUILD_OPT
#OPTIMIZER		= -Olimit 4000
OPTIMIZER		= -O -F Olimit,4000
endif
MKSHLIB			= $(LD) -G -z defs -h $(@:$(OBJDIR)/%.so=%.so)
#DSO_LDOPTS		= -G -W l,-Blargedynsym
endif

ifeq ($(CPU_ARCH),x86)
DEFINES			+= -Di386
endif

ODD_CFLAGS		+= -DSVR4 -DSNI -DRELIANTUNIX -Dsinix -DHAVE_SVID_GETTOD

# On SINIX 5.43, need to define IP_MULTICAST in order to get the
# IP multicast macro and struct definitions in netinet/in.h.
# (SINIX 5.42 does not have IP multicast at all.)
ifeq ($(OS_RELEASE),5.43)
ODD_CFLAGS		+= -DIP_MULTICAST
endif

RANLIB			= /bin/true

# For purify
NOMD_OS_CFLAGS		= $(ODD_CFLAGS)

# we do not have -MDupdate ...
OS_CFLAGS		= $(NOMD_OS_CFLAGS)
OS_LIBS			= -lsocket -lnsl -lresolv -ldl -lc
NOSUCHFILE		= /no-such-file

DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
