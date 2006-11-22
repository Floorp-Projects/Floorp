# ***** BEGIN LICENSE BLOCK ***** 
# Version: MPL 1.1/GPL 2.0/LGPL 2.1 
#
# The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
# "License"); you may not use this file except in compliance with the License. You may obtain 
# a copy of the License at http://www.mozilla.org/MPL/ 
#
# Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
# WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
# language governing rights and limitations under the License. 
#
# The Original Code is [Open Source Virtual Machine.] 
#
# The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
# by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
# Reserved. 
#
# Contributor(s): Adobe AS3 Team
#
# Alternatively, the contents of this file may be used under the terms of either the GNU 
# General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
# License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
# LGPL are applicable instead of those above. If you wish to allow use of your version of this 
# file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
# version of this file under the terms of the MPL, indicate your decision by deleting provisions 
# above and replace them with the notice and other provisions required by the GPL or the 
# LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
# under the terms of any one of the MPL, the GPL or the LGPL. 
# 
# ***** END LICENSE BLOCK ***** */


# DEBUG can be set to YES to include debugging info, or NO otherwise
DEBUG := YES
DEBUGGER := YES

CC     := gcc
CXX    := g++
AR     := ar

MMGC_ROOT := ../../MMgc
AVMPLUS_ROOT := ../..
ZLIB_ROOT := ../../../../modules/zlib/src

LDFLAGS := -ldl

# avmplus and MMgc make files
include makecommon
include ../../MMgc/makecommon

#****************************************************************************
# Targets of the build
#****************************************************************************

AVMPLUS_OUTPUT := libavmplus.a
MMGC_OUTPUT := ${MMGC_ROOT}/libmmgc.a
SHELL_OUTPUT := avmplus

all: ${MMGC_OUTPUT} ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT}

#****************************************************************************
# Shell Source files
#****************************************************************************

SHELL_SRCS := ../../shell/TypedArrayClass.cpp \
	../../shell/SystemClass.cpp \
	../../shell/StringBuilderClass.cpp \
	../../shell/FileInputStream.cpp \
	../../shell/FileClass.cpp \
	../../shell/DomainClass.cpp \
	../../shell/DebugCLI.cpp \
	../../shell/DataIO.cpp \
	../../shell/ConsoleOutputStream.cpp \
	../../shell/ByteArrayGlue.cpp \
	../../shell/avmshell.cpp \
	../../extensions/DictionaryGlue.cpp \
	../../extensions/JavaGlue.cpp \
	unixcpuid.cpp

# Use this target to generate a list of source files
gensrclist : 
	`find ../../core -type f -name "*.cpp" -printf "\t%p \134\n" > src.lst`
	`find ../../regexp -type f -name "*.cpp" -printf "\t%p \134\n" >> src.lst`
	`find ../unix -type f -name "*.cpp" -printf "\t%p \134\n" >> src.lst`

#****************************************************************************
# Output
#****************************************************************************

AVMPLUS_OBJS := $(addsuffix .o,$(basename ${AVMPLUS_SRCS}))
MMGC_OBJS := $(addsuffix .o,$(basename ${MMGC_SRCS}))
SHELL_OBJS := $(addsuffix .o,$(basename ${SHELL_SRCS}))
ZLIB_OBJS := $(addsuffix .o,$(basename ${ZLIB_SRCS}))

${MMGC_OUTPUT}: ${MMGC_OBJS}
	${AR} rcs $@ ${MMGC_OBJS}
	@echo "MMgc lib Done"

${AVMPLUS_OUTPUT}: ${AVMPLUS_OBJS}
	${AR} rcs $@ ${AVMPLUS_OBJS}
	@echo "avmplus lib Done"

${SHELL_OUTPUT}: ${SHELL_OBJS} ${ZLIB_OBJS} ${AVMPLUS_OUTPUT}
	@echo "=== linking:" $@ "==="
	@${CXX} -o $@ ${LDFLAGS} ${SHELL_OBJS} ${ZLIB_OBJS} ${AVMPLUS_LIBS} ${EXTRA_LIBS}
	@echo "=== done:" $@ "==="

#****************************************************************************
# Dependencies
#****************************************************************************

ALL_SRCS := ${AVMPLUS_SRCS} ${SHELL_SRCS} ${MMGC_SRCS} ${ZLIB_SRCS}

ALL_OBJS := ${AVMPLUS_OBJS} ${SHELL_OBJS} ${MMGC_OBJS} ${ZLIB_OBJS}

# Create a lists of object and .d files to create -
# one for each source file in the directory
AVMPLUS_DEPS := ${patsubst %${strip .cpp},%.d,${ALL_SRCS}}

# make any non-existent make files (using the g++ preprocessor)
${AVMPLUS_DEPS} : %.d : %.cpp
	${CXX} ${CPPFLAGS} ${AVMPLUS_INCS} -MM $< > $@

#****************************************************************************
# Rules
#****************************************************************************


clean:
	-rm -f core* ${ALL_OBJS} ${DEPS} ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT} ${MMGC_OUTPUT} log outputdebug.txt
	-rm -Rf ../../**/*.s

clean-gc:
	-rm -f core* ${MMGC_OBJS} ${DEPS} ${MMGC_OUTPUT} ${AVMPLUS_OUTPUT} log outputdebug.txt

clean-vm:
	-rm -f core* ${AVMPLUS_OBJS} ${DEPS} ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT} log outputdebug.txt

clean-zlib:
	-rm -f core* ${ZLIB_OBJS} ${DEPS} log outputdebug.txt

install:
	@cp -vf ${SHELL_OUTPUT} /home/mtilburg/bin/ 

#ifneq (${MAKECMDGOALS},clean)
#include ${DEPS} # include the generated make files, which make the object files
#endif

