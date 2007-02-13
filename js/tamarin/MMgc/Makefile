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
# The Original Code is [Open Source Virtual Machine.].
#
# The Initial Developer of the Original Code is
# Adobe System Incorporated.
# Portions created by the Initial Developer are Copyright (C) 2004-2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Adobe AS3 Team
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


# DEBUG can be set to YES to include debugging info, or NO otherwise
DEBUG := YES

# DEBUGGER can be set to choose the Content Debugger version of the gc
DEBUGGER := YES

MMGC_ROOT := .

CC     := gcc
CXX    := g++
AR     := ar

UNAME  := $(shell uname)

ARCH := ${shell uname -m}

ifeq ($(UNAME),Linux)
	MMGC_PLATFORM_DEFS := -DSOFT_ASSERTS -D_DEBUG 
endif

ifeq ($(UNAME),Darwin)
   MMGC_PLATFORM_DEFS := -DDARWIN -D_MAC -Dpowerc -DSOFT_ASSERTS -DDEBUGGER -DTARGET_RT_MAC_MACHO=1 -D_DEBUG -DDEBUG
   ifeq (${ARCH},i386)
       MMGC_PLATFORM_DEFS := ${MMGC_PLATFORM_DEFS} -DAVMPLUS_IA32
   else
       MMGC_PLATFORM_DEFS := ${MMGC_PLATFORM_DEFS} -DAVMPLUS_PPC
   endif
endif

ifeq ($(UNAME),CYGWIN_NT-5.1)
   MMGC_PLATFORM_DEFS := -DCYGWIN -DAVMPLUS_IA32
endif

include makecommon

#****************************************************************************
# Targets of the build
#****************************************************************************

OUTPUT := libmmgc.a

all: ${OUTPUT}

#****************************************************************************
# Output
#****************************************************************************

OBJS := $(addsuffix .o,$(basename ${MMGC_SRCS}))

${OUTPUT}: ${OBJS}
	@echo "=== creating libmmgc.a ==="
	${AR} rcs $@ ${OBJS}
	@echo "Done"

#****************************************************************************
# Dependencies
#****************************************************************************

# Create a lists of object and .d files to create -
# one for each source file in the directory
DEPS := ${patsubst %${strip .cpp},%.d,${MMGC_SRCS}}

# make any non-existent make files (using the g++ preprocessor)
${DEPS} : %.d : %.cpp
	${CXX} ${MMGC_CXXFLAGS} ${MMGC_INCS} -MM $< > $@

# Rules

clean:
	-rm -f core* ${OBJS} ${OUTPUT} ${DEPS} log outputdebug.txt

ifneq (${MAKECMDGOALS},clean)
ifneq (${UNAME},Linux)
include ${DEPS} # include the generated make files, which make the object files
endif
endif
