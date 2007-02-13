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

# Set to YES if doing ARM skunkworks
ARM_SKUNKWORKS := YES

# Set to YES if using GNUARM toolchain
GNUARM := YES

# Set to YES if a bflt executable desired
BFLT := NO

CC     := arm-elf-gcc
CXX    := arm-elf-g++
AR     := arm-elf-ar

ifeq (YES, ${ARM_SKUNKWORKS})
  ifeq (YES, ${GNUARM})
    SOFT_FLOAT :=
    GNUARM_DEF := -DAVMPLUS_GNUARM
  else
    SOFT_FLOAT := -msoft-float
    GNUARM_DEF :=
  endif
else
  SOFT_FLOAT :=
  GNUARM_DEF :=
endif

DEBUG_CFLAGS     := -g ${SOFT_FLOAT} ${GNUARM_DEF}
RELEASE_CFLAGS   := -O3 ${SOFT_FLOAT} ${GNUARM_DEF}

ifeq (YES, ${BFLT})
  LDFLAGS := -elf2flt
else
  LDFLAGS := 
endif

DEBUG_CXXFLAGS   := ${DEBUG_CFLAGS} -fno-exceptions -fno-rtti
RELEASE_CXXFLAGS := ${RELEASE_CFLAGS} -fno-exceptions -fno-rtti

ifeq (YES, ${DEBUG})
   CFLAGS       := ${DEBUG_CFLAGS}
   CXXFLAGS     := ${DEBUG_CXXFLAGS}
else
   CFLAGS       := ${RELEASE_CFLAGS}
   CXXFLAGS     := ${RELEASE_CXXFLAGS}
endif

#****************************************************************************
# Preprocessor directives
#****************************************************************************

ifeq (YES, ${ARM_SKUNKWORKS})
  DEFS := -DUNIX -DCYGWIN -DAVMPLUS_ARM -DMMGC_ARM -DDEBUGGER -DENABLE_COMPILER -DAVMPLUS_CDECL -DAVMPLUS_UCLINUX
else
  DEFS := -DUNIX -DAVMPLUS_IA32 -DDEBUGGER -DENABLE_COMPILER -DAVMPLUS_CDECL -DAVMPLUS_LINUX
endif

CFLAGS   := ${CFLAGS} ${DEFS}
CXXFLAGS := ${CXXFLAGS} ${DEFS}

#****************************************************************************
# Include paths
#****************************************************************************

INCS := -I../../core -I../../MMgc -I../../codegen -I../../regexp -I../../../../../zlib

#****************************************************************************
# Libraries
#****************************************************************************

LIBS := -L. -L../../MMgc -lavmplus -lmmgc 
#-lpthread

#****************************************************************************
# Targets of the build
#****************************************************************************

AVMPLUS_OUTPUT := libavmplus.a

SHELL_OUTPUT := avmplus

all: ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT}

#****************************************************************************
# Source files
#****************************************************************************

SHELL_SRCS := ../../../../../zlib/zutil.cpp \
	../../../../../zlib/uncompr.cpp \
	../../../../../zlib/trees.cpp \
	../../../../../zlib/inftrees.cpp \
	../../../../../zlib/inflate.cpp \
	../../../../../zlib/inffast.cpp \
	../../../../../zlib/infback.cpp \
	../../../../../zlib/deflate.cpp \
	../../../../../zlib/crc32.cpp \
	../../../../../zlib/compress.cpp \
	../../../../../zlib/adler32.cpp \
	../../shell/TypedArrayClass.cpp \
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
	../../extensions/Dictionary.cpp

# Use this target to generate a list of source files
gensrclist : 
	`find ../../core -type f -name "*.cpp" -printf "\t%p \134\n" > src.lst`
	`find ../../regexp -type f -name "*.cpp" -printf "\t%p \134\n" >> src.lst`
	`find ../unix -type f -name "*.cpp" -printf "\t%p \134\n" >> src.lst`


AVMPLUS_SRCS := ../../core/AbcEnv.cpp \
	../../core/AbcGen.cpp \
	../../core/AbcParser.cpp \
	../../core/AbstractFunction.cpp \
	../../core/Accessor.cpp \
	../../core/ActionBlockConstants.cpp \
	../../core/ArrayClass.cpp \
	../../core/ArrayObject.cpp \
	../../core/AtomArray.cpp \
	../../core/AtomConstants.cpp \
	../../core/AvmCore.cpp \
	../../core/avmplus.cpp \
	../../core/avmplusDebugger.cpp \
	../../core/avmplusHashtable.cpp \
	../../core/avmplusProfiler.cpp \
	../../core/BigInteger.cpp \
	../../core/BooleanClass.cpp \
	../../core/builtin.cpp \
	../../codegen/ArmAssembler.cpp \
	../../codegen/CodegenMIR.cpp \
	../../core/BuiltinTraits.cpp \
	../../core/ClassClass.cpp \
	../../core/ClassClosure.cpp \
	../../core/Date.cpp \
	../../core/DateClass.cpp \
	../../core/DateObject.cpp \
	../../core/Domain.cpp \
	../../core/DomainEnv.cpp \
	../../core/DynamicProfiler.cpp \
	../../core/E4XNode.cpp \
	../../core/ErrorClass.cpp \
	../../core/ErrorConstants.cpp \
	../../core/Exception.cpp \
	../../core/FrameState.cpp \
	../../core/FunctionClass.cpp \
	../../core/GrowableBuffer.cpp \
	../../core/IntClass.cpp \
	../../core/Interpreter.cpp \
	../../core/MathClass.cpp \
	../../core/MathUtils.cpp \
	../../core/MethodClosure.cpp \
	../../core/MethodEnv.cpp \
	../../core/MethodInfo.cpp \
	../../core/Multiname.cpp \
	../../core/MultinameHashtable.cpp \
	../../core/Namespace.cpp \
	../../core/NamespaceClass.cpp \
	../../core/NamespaceSet.cpp \
	../../core/NativeFunction.cpp \
	../../core/NumberClass.cpp \
	../../core/ObjectClass.cpp \
	../../core/opcodes.cpp \
	../../core/PoolObject.cpp \
	../../core/PrintWriter.cpp \
	../../core/RegExpClass.cpp \
	../../core/RegExpObject.cpp \
	../../core/ScopeChain.cpp \
	../../core/ScriptBuffer.cpp \
	../../core/ScriptObject.cpp \
	../../core/StackTrace.cpp \
	../../core/StaticProfiler.cpp \
	../../core/StringBuffer.cpp \
	../../core/StringClass.cpp \
	../../core/StringObject.cpp \
	../../core/Toplevel.cpp \
	../../core/Traits.cpp \
	../../core/UnicodeUtils.cpp \
	../../core/Verifier.cpp \
	../../core/VTable.cpp \
	../../core/XMLClass.cpp \
	../../core/XMLListClass.cpp \
	../../core/XMLListObject.cpp \
	../../core/XMLObject.cpp \
	../../core/XMLParser16.cpp \
	../../regexp/chartables.cpp \
	../../regexp/pcre.cpp \
	../arm/DateUnix.cpp \
	../arm/MathUtilsUnix.cpp \
	../arm/OSDepUnix.cpp


#****************************************************************************
# Output
#****************************************************************************

AVMPLUS_OBJS := $(addsuffix .o,$(basename ${AVMPLUS_SRCS}))

${AVMPLUS_OUTPUT}: ${AVMPLUS_OBJS}
	${AR} rcs $@ ${AVMPLUS_OBJS}
	@echo "Done"

SHELL_OBJS := $(addsuffix .o,$(basename ${SHELL_SRCS}))

${SHELL_OUTPUT}: ${SHELL_OBJS} ${AVMPLUS_OUTPUT}
	@echo "=== linking:" $@ "==="
	@${CXX} -o $@ ${LDFLAGS} ${SHELL_OBJS} ${LIBS} ${EXTRA_LIBS}
	@echo "=== done:" $@ "==="

#****************************************************************************
# Dependencies
#****************************************************************************

ALL_SRCS := ${AVMPLUS_SRCS} ${SHELL_SRCS}

ALL_OBJS := ${AVMPLUS_OBJS} ${SHELL_OBJS}

# Create a lists of object and .d files to create -
# one for each source file in the directory
DEPS := ${patsubst %${strip .cpp},%.d,${ALL_SRCS}}

# make any non-existent make files (using the g++ preprocessor)
${DEPS} : %.d : %.cpp
	${CXX} ${CPPFLAGS} ${INCS} -MM $< > $@

#****************************************************************************
# Rules
#****************************************************************************

# Rules for compiling source files to object files
%.o : %.cpp
	@echo "=== compiling:" $@ ${OPT}
	@${CXX} -c ${OPT} ${CXXFLAGS} -Wa,-ahld=$(addsuffix .s, $(basename $<)) ${INCS} $< -o $@

clean:
	-rm -f core* ${ALL_OBJS} ${DEPS} ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT} log outputdebug.txt
	-rm -Rf ../../**/*.s

install:
	@cp -vf ${SHELL_OUTPUT} /home/mtilburg/bin/ 

#ifneq (${MAKECMDGOALS},clean)
#include ${DEPS} # include the generated make files, which make the object files
#endif

