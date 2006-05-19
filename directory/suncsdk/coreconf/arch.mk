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

#######################################################################
# Master "Core Components" macros for getting the OS architecture     #
#######################################################################

#
# Macros for getting the OS architecture
#

OS_ARCH		:= $(subst /,_,$(shell uname -s))
OS_RELEASE	:= $(shell uname -r)
OS_TEST		:= $(shell uname -m)

#
# No -p option in HPUX uname and since we 
# do not build for Itanic skip it for now
#

ifneq ($(OS_ARCH),HP-UX)
CPU_ARCH := $(shell uname -p)
endif

#
# Force the IRIX64 machines to use IRIX.
#

ifeq ($(OS_ARCH),IRIX64)
	OS_ARCH = IRIX
endif

#
# Force the newer BSDI versions to use the old arch name.
#

ifeq ($(OS_ARCH),BSD_OS)
	OS_ARCH = BSD_386
endif

#
# Drop all but the major revision for FreeBSD releases.
#
# Handle one dot (2.2-STABLE) and two dot (2.2.5-RELEASE) forms.
#

ifeq ($(OS_ARCH),FreeBSD)
	OS_RELEASE := $(basename $(shell uname -r))
	NEW_OS_RELEASE := $(basename $(OS_RELEASE))
	ifneq ($(NEW_OS_RELEASE),"")
		OS_RELEASE := $(NEW_OS_RELEASE)
	endif
endif

#
# Catch NCR butchering of SVR4
#

ifeq ($(OS_ARCH),UNIX_SV)
	ifneq ($(findstring NCR, $(shell grep NCR /etc/bcheckrc | head -1 )),)
		OS_ARCH = NCR
	else
		# Make UnixWare something human readable
		OS_ARCH = UNIXWARE
	endif

	# Get the OS release number, not 4.2
	OS_RELEASE := $(shell uname -v)
endif

ifeq ($(OS_ARCH),UNIX_System_V)
	OS_ARCH	= NEC
endif

ifeq ($(OS_ARCH),AIX)
	OS_RELEASE := $(shell uname -v).$(shell uname -r)
endif

#
# Distinguish between OSF1 V4.0B and V4.0D
#

ifeq ($(OS_ARCH)$(OS_RELEASE),OSF1V4.0)
	OS_VERSION := $(shell uname -v)
	ifeq ($(OS_VERSION),564)
		OS_RELEASE := V4.0B
	endif
	ifeq ($(OS_VERSION),878)
		OS_RELEASE := V4.0D
	endif
endif

ifeq (osfmach3,$(findstring osfmach3,$(OS_RELEASE)))
	OS_RELEASE	= ppc
endif

#
# SINIX changes name to ReliantUNIX with 5.43
#

ifeq ($(OS_ARCH),ReliantUNIX-N)
	OS_ARCH    = ReliantUNIX
	OS_RELEASE = 5.4
endif

ifeq ($(OS_ARCH),SINIX-N)
	OS_ARCH    = ReliantUNIX
	OS_RELEASE = 5.4
endif

#
# OS_OBJTYPE is used only by Linux
#
# No longer used - mahesht 8/24/98

ifeq ($(OS_ARCH),Linux)
# OS_OBJTYPE  = ELF
#  OS_RELEASE := $(basename $(OS_RELEASE))
  ifeq (2.6,$(findstring 2.6,$(OS_RELEASE)))
    OS_RELEASE      :=2.6
  endif
  ifeq (2.4,$(findstring 2.4,$(OS_RELEASE)))
    OS_RELEASE      :=2.4
  endif
  ifeq ($(USE_LIBC),1)
    LIBC_TAG := _libc
  else
    LIBC_TAG := _glibc
  endif
endif

#######################################################################
# Master "Core Components" macros for getting the OS target           #
#######################################################################

#
# Note: OS_TARGET should be specified on the command line for gmake.
# When OS_TARGET=WIN95 is specified, then a Windows 95 target is built.
# The difference between the Win95 target and the WinNT target is that
# the WinNT target uses Windows NT specific features not available
# in Windows 95. The Win95 target will run on Windows NT, but (supposedly)
# at lesser performance (the Win95 target uses threads; the WinNT target
# uses fibers).
#
# When OS_TARGET=WIN16 is specified, then a Windows 3.11 (16bit) target
# is built. See: win16_3.11.mk for lots more about the Win16 target.
#
# If OS_TARGET is not specified, it defaults to $(OS_ARCH), i.e., no
# cross-compilation.
#

#
# The following hack allows one to build on a WIN95 machine (as if
# s/he were cross-compiling on a WINNT host for a WIN95 target).
# It also accomodates for MKS's uname.exe.  If you never intend
# to do development on a WIN95 machine, you don't need this hack.
#
ifeq ($(OS_ARCH),WIN95)
	OS_ARCH   = WINNT
	OS_TARGET = WIN95
endif
ifeq ($(OS_ARCH),Windows_95)
	OS_ARCH   = Windows_NT
	OS_TARGET = WIN95
endif

#
# On WIN32, we also define the variable CPU_ARCH.
#

ifeq ($(OS_ARCH), WINNT)
	CPU_ARCH := $(shell uname -p)
	ifeq ($(CPU_ARCH),I386)
		CPU_ARCH = x386
	endif
else
#
# If uname -s returns "Windows_NT", we assume that we are using
# the uname.exe in MKS toolkit.
#
# The -r option of MKS uname only returns the major version number.
# So we need to use its -v option to get the minor version number.
# Moreover, it doesn't have the -p option, so we need to use uname -m.
#
ifeq ($(OS_ARCH), Windows_NT)
	OS_ARCH = WINNT
	OS_MINOR_RELEASE := $(shell uname -v)
	ifeq ($(OS_MINOR_RELEASE),00)
		OS_MINOR_RELEASE = 0
	endif
	OS_RELEASE = $(OS_RELEASE).$(OS_MINOR_RELEASE)
	CPU_ARCH := $(shell uname -m)
	#
	# MKS's uname -m returns "586" on a Pentium machine.
	#
	ifneq (,$(findstring 86,$(CPU_ARCH)))
		CPU_ARCH = x386
	endif
endif
endif

ifndef OS_TARGET
	OS_TARGET = $(OS_ARCH)
endif

ifeq ($(OS_TARGET), WIN95)
	OS_RELEASE = 4.0
endif

ifeq ($(OS_TARGET), WIN16)
	OS_RELEASE =
#	OS_RELEASE = _3.11
endif

#
# This variable is used to get OS_CONFIG.mk.
#

ifeq ($(OS_TEST),i86pc)
	OS_CONFIG = $(OS_TARGET)$(OS_OBJTYPE)$(OS_RELEASE)_$(OS_TEST)
else
	OS_CONFIG = $(OS_TARGET)$(OS_OBJTYPE)$(OS_RELEASE)
endif

ifeq ($(OS_TARGET), SOLARIS)
ifdef USE_64
	64BIT_TAG=_64
else
	64BIT_TAG=
endif
endif

#
# OBJDIR_TAG depends on the predefined variable BUILD_OPT,
# to distinguish between debug and release builds.
#

ifdef BUILD_OPT
	ifeq ($(OS_TARGET),WIN16)
		OBJDIR_TAG = _O
	else
		OBJDIR_TAG = $(64BIT_TAG)_OPT
	endif
else
	ifeq ($(OS_TARGET),WIN16)
		OBJDIR_TAG = _D
	else
		OBJDIR_TAG = $(64BIT_TAG)_DBG
	endif
endif

#
# The following flags are defined in the individual $(OS_CONFIG).mk
# files.
#
# CPU_TAG is defined if the CPU is not the most common CPU.
# COMPILER_TAG is defined if the compiler is not the native compiler.
# IMPL_STRATEGY may be defined too.
#

# Name of the binary code directories
ifeq ($(OS_ARCH), WINNT)
	ifeq ($(CPU_ARCH),x386)
		OBJDIR_NAME = $(OS_CONFIG)$(OBJDIR_TAG).OBJ
	else
		OBJDIR_NAME = $(OS_CONFIG)$(CPU_ARCH)$(OBJDIR_TAG).OBJ
	endif
else
endif

OBJDIR_NAME = $(OS_CONFIG)$(CPU_TAG)$(COMPILER_TAG)$(LIBC_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJ

ifeq ($(OS_ARCH), WINNT)
ifneq ($(OS_TARGET),WIN16)
ifndef BUILD_OPT
#
# Define USE_DEBUG_RTL if you want to use the debug runtime library
# (RTL) in the debug build
#
ifdef USE_DEBUG_RTL
	OBJDIR_NAME = $(OS_CONFIG)$(CPU_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)$(OBJDIR_TAG).OBJD
endif
endif
endif
endif

