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

DEPTH=../..

include $(DEPTH)/config/config.mk

PROGRAM = idlc

CPPSRCS =		     \
	main.cpp	     \
	IdlParser.cpp	     \
	IdlScanner.cpp	     \
	Exceptions.cpp	     \
	Id.cppect.cpp	     \
	IdlVariable.cpp	     \
	IdlAttribute.cpp     \
	IdlEnum.cpp	     \
	IdlFunction.cpp	     \
	IdlInterface.cpp     \
	IdlParameter.cpp     \
	IdlSpecification.cpp \
	FileGen.cpp	     \
	XPCOMGen.cpp	     \
	JSStubGen.cpp	     \
	$(NULL)

OBJS	= $(CPPSRCS:.cpp=.o)

EX_LIBS = \
	$(NULL)

NON_DIRS = $(PROGS)
TARGETS = $(NON_DIRS)

include $(DEPTH)/config/rules.mk

$(OBJDIR)/%.o: %.cpp
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ $(CFLAGS) -DUSE_NSREG -c $*.cpp

export::

install:: $(PROGRAM)
	$(INSTALL) $(PROGRAM) $(DIST)/bin

