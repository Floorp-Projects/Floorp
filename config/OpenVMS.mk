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

# We don't want -KPIC as it forces the compiler to generate a .i file.
DSO_CFLAGS	=

# We don't want the standard set of UNIX libraries.
OS_LIBS		=

# Define VMS
OS_CFLAGS	+= -DVMS
OS_CXXFLAGS	+= -DVMS

# If we are building POSIX images, then force it.
ifdef INTERNAL_TOOLS
CC		= c89
CCC		= cxx
OS_CFLAGS	= $(ACDEFINES) -O -Wc,names=short
OS_CXXFLAGS	= $(ACDEFINES) -O -Wc,names=short
OS_LDFLAGS	=
endif

# NSPR stuff
# This is not needed if we have configure accept with-nspr correctly
ifdef VMS_NEVER_EVER
ifndef VMS_NSPR_ROOT
VMS_NSPR_ROOT		= /mozilla/mozilla/dist
endif
NSPR_INCLUDE_DIR	= $(VMS_NSPR_ROOT)/include
NSPR_CFLAGS		= -I$(NSPR_INCLUDE_DIR)
NSPR_LIBS		= -L$(VMS_NSPR_ROOT)/lib -lplds3 -lplc3 -lnspr3
MOZ_NATIVE_NSPR		= 1 # unfortunately defined too late
endif
