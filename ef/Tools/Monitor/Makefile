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

DEPTH = ../..

CPPSRCS = 		Breakpoints.cpp			\
				CommandLine.cpp			\
				Commands.cpp			\
				DataOutput.cpp			\
				Debugee.cpp				\
				main.cpp				\
				MonitorExpression.cpp	\
				Symbols.cpp				\
				Win32Util.cpp			\
				CatchAssert.cpp			\
				$(NULL)

LIBRARIES    =	Readline 				\
				DebuggerChannel 		\
				DisassembleX86			\
				libnspr21				\
				$(NULL)

PROGRAM = furmon

include $(DEPTH)/config/config.mk

ifeq ($(OS_ARCH), WINNT)
LDFLAGS += user32.lib kernel32.lib imagehlp.lib 
#$(DIST)/lib/Readline.lib $(DIST)/lib/DebuggerChannel.lib $(DIST)/lib/DisassembleX86.lib $(NSPR_LIBS) 
endif

include $(DEPTH)/config/rules.mk

libs:: $(PROGRAM)

