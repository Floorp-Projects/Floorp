#!gmake
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

