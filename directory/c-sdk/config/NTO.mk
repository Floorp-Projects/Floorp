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
# Portions created by the Initial Developer are Copyright (C) 1999-2000
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

######################################################################
# Config stuff for Neutrino
######################################################################

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
CLASSIC_NSPR = 1
endif
endif

#
# The default implementation strategy for Linux is pthreads.
#
ifeq ($(CLASSIC_NSPR),1)
IMPL_STRATEGY		= _EMU
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
else
USE_PTHREADS		= 1
IMPL_STRATEGY		= _PTH
DEFINES			+= -D_REENTRANT
endif


AR			 = qcc -Vgcc_ntox86 -M -a $@
CC			 = qcc -Vgcc_ntox86
LD			 = $(CC)
CCC			 = $(CC)

# Old Flags  -DNO_REGEX   -DSTRINGS_ALIGNED

OS_CFLAGS	 = -Wc,-Wall -Wc,-Wno-parentheses -DNTO  \
			   -D_QNX_SOURCE -DHAVE_POINTER_LOCALTIME_R -shared

COMPILER_TAG = _qcc
MKSHLIB		 = qcc -Vgcc_ntox86 -shared -Wl,-h$(@:$(OBJDIR)/%.so=%.so) -M

RANLIB		 = ranlib
G++INCLUDES	 =
OS_LIBS		 =
EXTRA_LIBS  = -lsocket

ifdef BUILD_OPT
OPTIMIZER = -O1
else
OPTIMIZER = -O1 -gstabs
endif

NOSUCHFILE	= /no-such-file

GARBAGE		+= *.map

