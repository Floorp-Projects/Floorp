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

OS_ARCH := $(subst /,_,$(shell uname -s))

#
# Attempt to differentiate between sparc and x86 Solaris
#

OS_TEST := $(shell uname -m)
ifeq ($(OS_TEST),i86pc)
        OS_RELEASE := $(shell uname -r)_$(OS_TEST)
else
        OS_RELEASE := $(shell uname -r)
endif

ifeq ($(OS_ARCH),SunOS)
	CPU_ARCH = $(shell uname -p)
endif


#
# Force the IRIX64 machines to use IRIX.
#
ifeq ($(OS_ARCH),IRIX)
	CPU_ARCH = mips
endif


ifeq ($(OS_ARCH),AIX)
	CPU_ARCH = ppc
        OS_RELEASE := $(shell uname -v).$(shell uname -r)
endif

#
# Force the newer BSDI versions to use the old arch name.
#

ifeq ($(OS_ARCH),BSD_OS)
        OS_ARCH = BSD_386
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
        OS_ARCH = NEC
endif

#
# Handle FreeBSD 2.2-STABLE and Linux 2.0.30-osfmach3
#
# OS_OBJTYPE is used only by Linux and FreeBSD
#
 
ifeq (,$(filter-out Linux FreeBSD,$(OS_ARCH)))
OS_RELEASE      := $(shell echo $(OS_RELEASE) | sed 's/-.*//')
endif

ifeq ($(OS_ARCH),FreeBSD)
	OS_OBJTYPE := ELF
	CPU_ARCH = x86
endif

ifeq ($(OS_ARCH),Linux)
        OS_RELEASE := $(basename $(OS_RELEASE))
	OS_OBJTYPE := ELF
	CPU_ARCH := $(shell uname -m)
	ifneq (,$(findstring 86,$(CPU_ARCH)))
		CPU_ARCH = x86
	endif
endif

ifeq ($(OS_ARCH),HP-UX)
	CPU_ARCH = hppa
endif

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

ifeq ($(OS_ARCH),WINNT)
	CPU_ARCH = $(shell uname -p)
	ifeq ($(CPU_ARCH),I386)
		CPU_ARCH = x86
	endif
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
                CPU_ARCH = x86
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
#       OS_RELEASE = _3.11
endif

#
# This variable is used to get OS_CONFIG.mk.
#

OS_CONFIG = $(OS_TARGET)$(OS_RELEASE)

#
# OBJDIR_TAG depends on the predefined variable BUILD_OPT,
# to distinguish between debug and release builds.
#

ifdef BUILD_OPT
        ifeq ($(OS_TARGET),WIN16)
                OBJDIR_TAG = _O
        else
                OBJDIR_TAG = _OPT
        endif
else
        ifeq ($(OS_TARGET),WIN16)
                OBJDIR_TAG = _D
        else
                OBJDIR_TAG = _DBG
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
        ifeq ($(CPU_ARCH),x86)
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
        OBJDIR_NAME = $(OS_CONFIG)$(CPU_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)$(OBJ
DIR_TAG).OBJD
endif
endif
endif
endif
