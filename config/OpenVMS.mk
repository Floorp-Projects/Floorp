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
OS_CFLAGS	+= -DVMS
OS_CXXFLAGS	+= -DVMS

# If we are building POSIX images, then force it.
ifdef INTERNAL_TOOLS
CC		= c89
CCC		= cxx
OS_CFLAGS	= $(ACDEFINES) -O -Wc,names=\(short,as\) -DAS_IS
OS_CXXFLAGS	= $(ACDEFINES) -O -Wc,names=\(short,as\) -DAS_IS
OS_LDFLAGS	=
endif

# This is where our Sharable Image trickery goes.
ifdef BUILD_OPT
VMS_DEBUG	= -O
else
VMS_DEBUG	= -g
endif
AS		= vmsas $(VMS_DEBUG)
LD		= vmsld MODULE=$(LIBRARY_NAME) DIST=$(DIST) \
		  DISTNSPR=$(subst -L/,/,$(NSPR_LIBS:-l%=)) $(VMS_DEBUG)
DSO_LDOPTS	=
EXTRA_DSO_LDOPTS+= $(EXTRA_LIBS)
MKSHLIB		= $(LD)
