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

DEPTH			= ../../..

MODULE_NAME		= sparc

include $(DEPTH)/config/config.mk

INCLUDES		+=										\
	-I$(DEPTH)/Utilities/General						\
	-I$(DEPTH)/Utilities/zlib							\
	-I$(DEPTH)/Runtime/ClassReader						\
	-I$(DEPTH)/Compiler/FrontEnd						\
	-I$(DEPTH)/Runtime/NativeMethods					\
	-I$(DEPTH)/Runtime/System							\
	-I$(DEPTH)/Compiler/PrimitiveGraph					\
	-I$(DEPTH)/Runtime/ClassInfo						\
	-I$(DEPTH)/Runtime/FileReader						\
	-I$(DEPTH)/Compiler/CodeGenerator					\
	-I$(DEPTH)/Compiler/CodeGenerator/md				\
	-I$(DEPTH)/Compiler/RegisterAllocator				\
	$(NULL)

CXXSRCS			= 										\
	SparcInstruction.cpp								\
	SparcSunOSEmitter.cpp								\
	sparc.nad.burg.cpp									\
	$(NULL)

include $(DEPTH)/config/rules.mk

export:: sparc.nad.burg.cpp

libs:: $(MODULE)

#
# Rules to generate sparc.nad.burg.[cpp][h]
#

sparc.nad.burg.cpp: sparc.nad.burg $(BURG)
	$(BURG) -I -o $@ < $<

sparc.nad.burg: sparc.nad $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations $(DEPTH)/Tools/Nad/nad.pl
	$(PERL) $(DEPTH)/Tools/Nad/nad.pl $< $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations	\
		$(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.h									\
		$(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.cpp								\
		$<.burg.h > $@

#
# Extra cleaning
#

clobber::
	rm -f sparc.nad.burg.cpp sparc.nad.burg.h sparc.nad.burg

realclean clobber_all::
	rm -f sparc.nad.burg.cpp sparc.nad.burg.h sparc.nad.burg
