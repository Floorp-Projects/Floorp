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

CPPSRCS = 		PPC601AppleMacOSEmitter.cpp		\
			PPCInstructions.cpp			\
			ppc601-macos.nad.burg.cpp		\
			$(NULL)

LOCAL_MD_EXPORTS_ppc = 	AssembledInstructions.h			\
			PPC601AppleMacOSEmitter.h		\
			PPC601AppleMacOS_Support.h		\
			PPC601Cpu.h				\
			PPCCalls.h				\
			PPCInstructionTemplates.h		\
			PPCInstructions.h			\
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

#
# Rules to obtain burg-generated files
#
export:: ppc601-macos.nad.burg.cpp


#
# Rules to generate ppc601-macos.nad.burg.[cpp][h]
#

ppc601-macos.nad.burg.cpp: ppc601-macos.nad.burg $(BURG)
	$(BURG) -I -o $@ < $<

ppc601-macos.nad.burg: ppc601-macos.nad $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations $(DEPTH)/Tools/Nad/nad.pl
	$(PERL) $(DEPTH)/Tools/Nad/nad.pl $< $(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations	\
		$(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.h									\
		$(DEPTH)/Compiler/PrimitiveGraph/PrimitiveOperations.cpp								\
		$<.burg.h > $@

#
# Extra cleaning
#

clobber::
	rm -f ppc601-macos.nad.burg.cpp ppc601-macos.nad.burg.h ppc601-macos.nad.burg

realclean clobber_all::
	rm -f ppc601-macos.nad.burg.cpp ppc601-macos.nad.burg.h ppc601-macos.nad.burg

