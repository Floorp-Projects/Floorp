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


######################################################################
# Config stuff for BeOS (all architectures)
######################################################################

######################################################################
# Version-independent
######################################################################

DEFINES			+= 
XP_DEFINE		= -DXP_BEOS

OBJ_SUFFIX              = o
LIB_SUFFIX              = a
DLL_SUFFIX              = so
AR                      = ar cr $@

ifdef BUILD_OPT
DEFINES                 = -UDEBUG -DNDEBUG
OBJDIR_TAG              = _OPT
else
DEFINES                 = -DDEBUG -UNDEBUG
OBJDIR_TAG              = _DBG
endif

ifeq (PC,$(findstring PC,$(OS_TEST)))
CPU_ARCH		= x86
CC                      = gcc
CCC                     = g++
LD                      = gcc
RANLIB                  = ranlib
DSO_LDOPTS              = -nostart
PORT_FLAGS		= -DHAVE_STRERROR
ifdef BUILD_OPT
OPTIMIZER		= -O2
LDFLAGS			+= -s
else
OPTIMIZER		= -gdwarf-2 -O0
endif
else
CPU_ARCH		= ppc
CC                      = mwcc
CCC                     = mwcc
LD                      = mwld
RANLIB                  = ranlib
DSO_LDOPTS              = -xms -export pragma \
					-init _init_routine_ \
					-term _term_routine_ \
					-lroot -lnet \
					/boot/develop/lib/ppc/glue-noinit.a \
					/boot/develop/lib/ppc/init_term_dyn.o \
					/boot/develop/lib/ppc/start_dyn.o 

PORT_FLAGS		= -DHAVE_STRERROR -D_POSIX_SOURCE
ifdef BUILD_OPT
OPTIMIZER		= -O2
else
OPTIMIZER		= -g -O0
endif
endif
CPU_ARCH_TAG		= _$(CPU_ARCH)

OS_INCLUDES		=  -I- -I. 
#G++INCLUDES		= -I/usr/include/g++

PLATFORM_FLAGS		= -DBeOS -DBEOS $(OS_INCLUDES)

OS_CFLAGS		= $(DSO_CFLAGS) $(PLATFORM_FLAGS) $(PORT_FLAGS)

USE_BTHREADS = 1

MKSHLIB			= $(LD) $(DSO_LDOPTS)

OBJDIR_NAME	= $(OS_CONFIG)_$(CPU_ARCH)$(OBJDIR_TAG).OBJ

####################################################################
#
# One can define the makefile variable NSDISTMODE to control
# how files are published to the 'dist' directory.  If not
# defined, the default is "install using relative symbolic
# links".  The two possible values are "copy", which copies files
# but preserves source mtime, and "absolute_symlink", which
# installs using absolute symbolic links.  The "absolute_symlink"
# option requires NFSPWD.
#
####################################################################

NSINSTALL       = $(MOD_DEPTH)/config/$(OBJDIR_NAME)/nsinstall

ifeq ($(NSDISTMODE),copy)
# copy files, but preserve source mtime
INSTALL         = $(NSINSTALL) -t
else
ifeq ($(NSDISTMODE),absolute_symlink)
# install using absolute symbolic links
INSTALL         = $(NSINSTALL) -L `$(NFSPWD)`
else
# install using relative symbolic links
INSTALL         = $(NSINSTALL) -R
endif
endif

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
endef
