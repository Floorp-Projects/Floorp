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

DEPTH = ../..

DIRS = md
SUBMODULES = 	$(DIRS)

CPPSRCS = 	Backend.cpp						\
		CGScheduler.cpp						\
		CodeGenerator.cpp					\
		IGVisualizer.cpp					\
		Instruction.cpp						\
		InstructionEmitter.cpp					\
		NativeCodeCache.cpp					\
		NativeFormatter.cpp					\
		HTMLMethodDump.cpp					\
		Scheduler.cpp						\
		ExceptionTable.cpp					\
		$(NULL)

LOCAL_EXPORTS = Backend.h						\
		Burg.h							\
		CGScheduler.h						\
		CodeGenerator.h						\
		ControlNodeScheduler.h					\
		ExceptionTable.h					\
		FormatStructures.h					\
		HTMLMethodDump.h					\
		IGVisualizer.h						\
		Instruction.h						\
		InstructionEmitter.h					\
		NativeCodeCache.h					\
		NativeFormatter.h					\
		Scheduler.h						\
		$(NULL)

MODULE_NAME = EF

#######################################################################
# (2) Include "component" configuration information.                  #
#######################################################################

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################



#######################################################################
# (4) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(DEPTH)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

# Special rules for Instruction.cpp for gcc
ifneq ($(OS_ARCH),WINNT)

$(OBJDIR)/Instruction$(OBJ_SUFFIX): Instruction.cpp
	$(CCC) -c $(filter-out $(EXC_FLAGS),$(CFLAGS)) $< -o $@

endif
