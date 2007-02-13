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

CC     := gcc
CXX    := g++
AR     := ar

DEBUG_CFLAGS     := -g -D_DEBUG
RELEASE_CFLAGS   := -O2

DEBUG_CXXFLAGS   := ${DEBUG_CFLAGS} -fno-exceptions -fno-rtti -fcheck-new
RELEASE_CXXFLAGS := ${RELEASE_CFLAGS} -fno-exceptions -fno-rtti -fcheck-new

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

ARCH := $(shell uname -m)

DEFS := -DMEMORY_INFO -DSOFT_ASSERTS -D_MAC -DAVMPLUS_MAC -DAVMPLUS_SHELL -DDARWIN -DUNIX -DDEBUGGER -DENABLE_COMPILER -DTARGET_RT_MAC_MACHO=1

ifeq (i386, ${ARCH})
   DEFS := -DAVMPLUS_IA32 ${DEFS}
else
   DEFS := -DAVMPLUS_PPC ${DEFS}
endif

CFLAGS   := ${CFLAGS} ${DEFS}
CXXFLAGS := ${CXXFLAGS} ${DEFS}

#****************************************************************************
# Include paths
#****************************************************************************

INCS := -I../../core -I../../MMgc -I../../extensions -I../../codegen -I../../pcre -I../../shell

#****************************************************************************
# Libraries
#****************************************************************************

LIBS := -L. -L../../MMgc -lavmplus -lmmgc -framework Carbon

#****************************************************************************
# Targets of the build
#****************************************************************************

AVMPLUS_OUTPUT := libavmplus.a

SHELL_OUTPUT := avmplus.out

all: ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT}

#****************************************************************************
# Source files
#****************************************************************************

SHELL_SRCS := ../../shell/avmshell.cpp \
              ../../shell/ByteArrayGlue.cpp \
              ../../shell/DataIO.cpp \
              ../../shell/DomainClass.cpp \
              ../../shell/TypedArrayClass.cpp \
              ../../shell/ConsoleOutputStream.cpp \
              ../../shell/FileClass.cpp \
              ../../shell/FileInputStream.cpp \
			  ../../shell/DebugCLI.cpp \
              ../../shell/StringBuilderClass.cpp \
              ../../shell/SystemClass.cpp \
		../../extensions/DictionaryGlue.cpp

AVMPLUS_SRCS := \
        ../../core/AbcEnv.cpp \
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
        ../mac/AvmDebugMac.cpp \
        ../../core/BigInteger.cpp \
        ../../core/BooleanClass.cpp \
        ../../core/BuiltinTraits.cpp \
        ../../core/ClassClass.cpp \
        ../../core/ClassClosure.cpp \
        ../../codegen/Ia32Assembler.cpp \
        ../../codegen/PpcAssembler.cpp \
        ../../codegen/CodegenMIR.cpp \
        ../../core/Date.cpp \
        ../unix/DateUnix.cpp \
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
        ../../core/GrowableBuffer.cpp \
        ../../core/FunctionClass.cpp \
        ../../core/IntClass.cpp \
        ../../core/Interpreter.cpp \
        ../../core/MathClass.cpp \
        ../../core/MathUtils.cpp \
        ../unix/MathUtilsUnix.cpp \
        ../../core/MethodClosure.cpp \
        ../../core/MethodEnv.cpp \
        ../../core/MethodInfo.cpp \
        ../../core/Multiname.cpp \
        ../../core/MultinameHashtable.cpp \
        ../../core/Namespace.cpp \
        ../../core/NamespaceSet.cpp \
        ../../core/NamespaceClass.cpp \
        ../../core/NativeFunction.cpp \
        ../mac/NativeFunctionMac.cpp \
        ../../core/NumberClass.cpp \
        ../../core/ObjectClass.cpp \
        ../unix/OSDepUnix.cpp \
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
        ../../core/avmplus.cpp \
        ../../core/avmplusDebugger.cpp \
        ../../core/avmplusHashtable.cpp \
        ../../pcre/pcre_chartables.cpp \
        ../../pcre/pcre_compile.cpp \
        ../../pcre/pcre_config.cpp \
        ../../pcre/pcre_exec.cpp \
        ../../pcre/pcre_fullinfo.cpp \
        ../../pcre/pcre_get.cpp \
        ../../pcre/pcre_globals.cpp \
        ../../pcre/pcre_info.cpp \
        ../../pcre/pcre_ord2utf8.cpp \
        ../../pcre/pcre_refcount.cpp \
        ../../pcre/pcre_study.cpp \
        ../../pcre/pcre_tables.cpp \
        ../../pcre/pcre_try_flipped.cpp \
        ../../pcre/pcre_valid_utf8.cpp \
        ../../pcre/pcre_version.cpp \
        ../../pcre/pcre_xclass.cpp \
	../../../zlib/adler32.cpp \
	../../../zlib/compress.cpp \
	../../../zlib/crc32.cpp \
	../../../zlib/deflate.cpp \
	../../../zlib/inflate.cpp \
	../../../zlib/inffast.cpp \
	../../../zlib/inftrees.cpp \
	../../../zlib/trees.cpp \
	../../../zlib/zutil.cpp

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
	${CXX} ${CXXFLAGS} ${INCS} -MM $< > $@

#****************************************************************************
# Rules
#****************************************************************************

# Rules for compiling source files to object files
%.o : %.cpp
	@echo "=== compiling:" $@ ${OPT}
	@${CXX} -c ${OPT} ${CXXFLAGS} ${INCS} $< -o $@

clean:
	-rm -f core* ${ALL_OBJS} ${DEPS} ${SHELL_OUTPUT} ${AVMPLUS_OUTPUT} log outputdebug.txt

ifneq (${MAKECMDGOALS},clean)
include ${DEPS} # include the generated make files, which make the object files
endif

