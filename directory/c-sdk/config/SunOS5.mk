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
# Config stuff for SunOS 5.x on sparc and x86
#

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
LOCAL_THREADS_ONLY = 1
endif
ifndef NS_USE_NATIVE
NS_USE_GCC = 1
endif
endif

#
# The default implementation strategy on Solaris is pthreads.
# Global threads only and local threads only are also available.
#
ifeq ($(GLOBAL_THREADS_ONLY),1)
  IMPL_STRATEGY = _NATIVE
  DEFINES += -D_PR_GLOBAL_THREADS_ONLY
else
  ifeq ($(LOCAL_THREADS_ONLY),1)
    IMPL_STRATEGY = _EMU
    DEFINES += -D_PR_LOCAL_THREADS_ONLY
  else
    USE_PTHREADS = 1
ifeq ($(HAVE_CCONF), 1) 
    IMPL_STRATEGY =
else
    IMPL_STRATEGY = _PTH
endif
  endif
endif

ifeq ($(NS_USE_GCC), 1)
CC			= gcc -Wall
CCC			= g++ -Wall
ASFLAGS			+= -Wa,-P
COMPILER_TAG		= _gcc
ifdef NO_MDUPDATE
OS_CFLAGS		= $(NOMD_OS_CFLAGS)
else
OS_CFLAGS		= $(NOMD_OS_CFLAGS) -MDupdate $(DEPENDENCIES)
endif
else
CC			= cc -v -xstrconst
CCC			= CC -Qoption cg -xstrconst
ASFLAGS			+= -Wa,-P
OS_CFLAGS		= $(NOMD_OS_CFLAGS)
#
# If we are building for a release, we want to put all symbol
# tables in the debug executable or share library instead of
# the .o files, so that our clients can run dbx on the debug
# library without having the .o files around.
#
ifdef BUILD_NUMBER
ifndef BUILD_OPT
OS_CFLAGS		+= -xs
endif
endif
endif

ifeq ($(USE_64),1)
ifndef INTERNAL_TOOLS
ifneq ($(NS_USE_GCC), 1)
CC			+= -xarch=v9
CCC			+= -xarch=v9
else
CC			+= -m64
CCC			+= -m64
endif # NS_USE_GCC
endif # INTERNAL_TOOLS
COMPILER_TAG		+= _64
else # USE_64
ifeq ($(HAVE_CCONF), 1)
COMPILER_TAG		=
else
COMPILER_TAG		+= _32
endif # HAVE_CCONF
endif # USE_64

RANLIB			= echo

OS_DEFINES		= -DSVR4 -DSYSV -D__svr4 -D__svr4__ -DSOLARIS

ifeq ($(OS_TEST),i86pc)
CPU_ARCH		= x86
COMPILER_TAG		= _i86pc
OS_DEFINES		+= -Di386
# The default debug format, DWARF (-g), is not supported by gcc
# on i386-ANY-sysv4/solaris, but the stabs format is.  It is
# assumed that the Solaris assembler /usr/ccs/bin/as is used.
# If your gcc uses GNU as, you do not need the -Wa,-s option.
ifndef BUILD_OPT
ifeq ($(NS_USE_GCC), 1)
OPTIMIZER		= -Wa,-s -gstabs
endif
endif
else
ifeq ($(HAVE_CCONF), 1)
CPU_ARCH		=
else
CPU_ARCH		= sparc
endif
endif

ifeq ($(HAVE_CCONF), 1)
CPU_ARCH_TAG		=
else
CPU_ARCH_TAG		= _$(CPU_ARCH)
endif

ifeq (5.5,$(findstring 5.5,$(OS_RELEASE)))
OS_DEFINES		+= -DSOLARIS2_5
else
ifeq (,$(filter-out 5.3 5.4,$(OS_RELEASE)))
OS_DEFINES		+= -D_PR_NO_LARGE_FILES
else
OS_DEFINES		+= -D_PR_HAVE_OFF64_T
# The lfcompile64(5) man page on Solaris 2.6 says:
#     For applications that do not wish to conform to the POSIX or
#     X/Open  specifications,  the  64-bit transitional interfaces
#     are available by default.  No compile-time flags need to  be
#     set.
# But gcc 2.7.2.x fails to define _LARGEFILE64_SOURCE by default.
# The native compiler, gcc 2.8.x, and egcs don't have this problem.
#ifeq ($(NS_USE_GCC), 1)
OS_DEFINES		+= -D_LARGEFILE64_SOURCE
#endif
endif
endif

ifneq ($(LOCAL_THREADS_ONLY),1)
OS_DEFINES		+= -D_REENTRANT -DHAVE_POINTER_LOCALTIME_R
endif

# Purify doesn't like -MDupdate
NOMD_OS_CFLAGS		= $(DSO_CFLAGS) $(OS_DEFINES) $(SOL_CFLAGS)

MKSHLIB			= $(LD) $(DSO_LDOPTS)

# ld options:
# -G: produce a shared object
# -z defs: no unresolved symbols allowed
DSO_LDOPTS		= -G -h $(notdir $@)

# -KPIC generates position independent code for use in shared libraries.
# (Similarly for -fPIC in case of gcc.)
ifeq ($(NS_USE_GCC), 1)
DSO_CFLAGS		= -fPIC
else
DSO_CFLAGS		= -KPIC
endif

NOSUCHFILE		= /no-such-file

#
# Library of atomic functions for UltraSparc systems
#
# The nspr makefiles build ULTRASPARC_LIBRARY (which contains assembly language
# implementation of the nspr atomic functions for UltraSparc systems) in addition
# to libnspr.so. (The actual name of the library is
# lib$(ULTRASPARC_LIBRARY)$(MOD_VERSION).so
#
# The actual name of the filter-library, recorded in libnspr.so, is set to the
# value of $(ULTRASPARC_FILTER_LIBRARY).
# For an application to use the assembly-language implementation, a link should be
# made so that opening ULTRASPARC_FILTER_LIBRARY results in opening
# ULTRASPARC_LIBRARY. This indirection requires the user to explicitly set up
# library for use on UltraSparc systems, thereby helping to avoid using it by
# accident on non-UltraSparc systems.
# The directory containing the ultrasparc libraries should be in LD_LIBRARY_PATH.
#
ifeq ($(OS_TEST),sun4u)
ULTRASPARC_LIBRARY = ultrasparc
ULTRASPARC_FILTER_LIBRARY = libatomic.so
DSO_LDOPTS		+= -f $(ULTRASPARC_FILTER_LIBRARY)
endif
