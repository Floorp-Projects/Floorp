#
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
#
# Even though we use AUTOCONF, there are just too many things that need
# fixing up to do it any other way than via an architecture specific file.
#
# If we're not using NSBUILDROOT, then make sure we use multiple object
# directories. We want this name to be relatively short, and to be different
# from what NSPR uses (so that we can wipe out Mozilla objects without
# wiping NSPR objects.

# We don't want -KPIC as it forces the compiler to generate a .i file.
DSO_PIC_CFLAGS	=

# We don't want the standard set of UNIX libraries.
OS_LIBS		=

# Define VMS
OS_CFLAGS	+= -DVMS -DVMS_AS_IS -Wc,names=\(short,as\)
OS_CXXFLAGS	+= -DVMS -DVMS_AS_IS -Wc,names=\(short,as\)

# If we are building POSIX images, then these HOST symbols get used.
# We don't want to compile any POSIX image debug, so always remove -g.
# xpild accvio's if built with -O, so don't.
HOST_CC		= c89
HOST_CXX	= cxx
ifeq ($(PROGRAM),xpidl)
HOST_CFLAGS	= $(filter-out -g -O,$(OS_CFLAGS)) -DGETCWD_CANT_MALLOC
else
HOST_CFLAGS	= $(filter-out -g -O,$(OS_CFLAGS)) -DGETCWD_CANT_MALLOC -O
endif
HOST_CXXFLAGS	= $(filter-out -g -O,$(OS_CXXFLAGS)) -O

# In addition, we want to lose the OS_FLAGS for POSIX builds.
ifdef INTERNAL_TOOLS
OS_LDFLAGS	=
endif

# Although IS_COMPONENT is set in the Makefile, for xpconnect it is not set
# until AFTER config.mk (and therefore this file) included, hence it appears
# as unset. So for this module only, set IS_COMPONENT here.
ifeq ($(MODULE),xpconnect)
IS_COMPONENT	= 1
endif

# This is where our Sharable Image trickery goes.
AS		= vmsas $(OS_CFLAGS)
ifdef IS_COMPONENT
OS_LDFLAGS	+= IS_COMPONENT SRCDIR=$(srcdir)
endif
LD		= vmsld $(OS_LDFLAGS)
DSO_LDOPTS	=
MKSHLIB		= $(LD)
