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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
# Config stuff for SunOS5.x
#

include $(GDEPTH)/gconfig/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
	LOCAL_THREADS_ONLY = 1
	ifndef NS_USE_NATIVE
		NS_USE_GCC = 1
	endif
endif

#
# The default implementation strategy for Solaris is classic nspr.
#
ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
else
	ifeq ($(LOCAL_THREADS_ONLY),1)
		IMPL_STRATEGY = _LOCAL
		DEFINES += -D_PR_LOCAL_THREADS_ONLY
	else
		DEFINES += -D_PR_GLOBAL_THREADS_ONLY
	endif
endif

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
	IMPL_STRATEGY =
endif

DEFAULT_COMPILER = cc

ifdef NS_USE_GCC
	CC         = gcc
	OS_CFLAGS += -Wall -Wno-format
	CCC        = g++
	CCC       += -Wall -Wno-format
	ASFLAGS	  += -x assembler-with-cpp
	ifdef NO_MDUPDATE
		OS_CFLAGS += $(NOMD_OS_CFLAGS)
	else
		OS_CFLAGS += $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
	endif
else
	CC         = cc
	CCC        = CC
	ASFLAGS   += -Wa,-P
	OS_CFLAGS += $(NOMD_OS_CFLAGS)
	ifndef BUILD_OPT
		OS_CFLAGS  += -xs
	endif

endif

INCLUDES   += -I/usr/dt/include -I/usr/openwin/include

RANLIB      = echo
CPU_ARCH    = sparc
OS_DEFINES += -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS

ifneq ($(LOCAL_THREADS_ONLY),1)
	OS_DEFINES		+= -D_REENTRANT
endif

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS += $(DSO_CFLAGS) $(OS_DEFINES) $(SOL_CFLAGS)

MKSHLIB  = $(LD)
MKSHLIB += $(DSO_LDOPTS)

# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
DSO_LDOPTS += -G

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifdef NS_USE_GCC
	DSO_CFLAGS += -fPIC
else
	DSO_CFLAGS += -KPIC
endif

HAVE_PURIFY  = 1
NOSUCHFILE   = /solaris-rm-f-sucks
