#!gmake
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

#######################################################################
# (1) Directory specific info                                         #
#######################################################################

DEPTH = ../../../..

CPPSRCS = 		x86.nad.burg.cpp			\
			x86ArgumentList.cpp			\
			x86Arith64.cpp				\
			x86Opcode.cpp				\
			x86Emitter.cpp				\
			x86Float.cpp				\
			x86Formatter.cpp			\
			x86GenStub.cpp				\
			x86Instruction.cpp			\
			x86StdCall.cpp				\
			$(OS_SUPPORT)				\
			$(NULL)

LOCAL_MD_EXPORTS_x86 = 	x86ArgumentList.h			\
			x86Float.h				\
			x86Formatter.h				\
			x86Opcode.h				\
			x86StdCall.h				\
			x86Cpu.h				\
			x86Emitter.h			\
			x86Instruction.h			\
			$(NULL)

MODULE_NAME = EF

#######################################################################
# (2) Include "component" configuration information.                  #
#######################################################################

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

#
# Generation of OS-specific support files
#
ifeq ($(OS_ARCH),WINNT)
OS_SUPPORT		= x86Stub.cpp
else
ASFILES			= x86Stub_gas.s
endif

#######################################################################
# (4) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(DEPTH)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

#
# Rules to generate x86.nad.burg.[cpp][h]
#
ifneq ($(OS_ARCH),WINNT)
x86.nad.burg.cpp: x86.nad.burg $(BURG)
	$(BURG) -I -o $@ < $<

x86.nad.burg: x86.nad $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations $(DEPTH)/Tools/Nad/nad.pl
	$(PERL) $(DEPTH)/Tools/Nad/nad.pl $< $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations	\
		$(LOCAL_EXPORT_DIR)/PrimitiveOperations.h						\
		$(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.cpp								\
		$<.burg.h > $@
else
x86.nad.burg.cpp: x86.nad $(BURG) $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations $(DEPTH)/Tools/Nad/nad.pl
	$(DEPTH)/config/genburg.bat $(BURG) $(DEPTH)/Tools/Nad/nad.pl $< $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations
	cp $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.h $(LOCAL_EXPORT_DIR)
endif

# special rule so that successive make's don't try to build
# $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations from
# $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.cpp
#
$(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations:
	@echo
#
# Extra cleaning
#
clean clobber realclean clobber_all::
	rm -f x86.nad.burg.cpp x86.nad.burg.h x86.nad.burg

#
# Special rules for x86Win32Support.cpp on Windows: it currently breaks the
# compiler if compiled with optimizations
#
ifeq ($(OS_ARCH),WINNT)
OPT_FLAG = -O2
$(OBJDIR)/x86Win32_Support$(OBJ_SUFFIX): x86Win32_Support.cpp
	$(CCC) -c $(filter-out $(OPT_FLAG),$(CFLAGS)) $< -Fo$@
endif

