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


CSRCS = 	be.c			\
		burs.c			\
		closure.c		\
		delta.c			\
		fe.c			\
		item.c			\
		lex.c			\
		list.c			\
		main.c			\
		map.c			\
		nonterminal.c		\
		operator.c		\
		pattern.c		\
		plank.c			\
		queue.c			\
		rule.c			\
		string.c		\
		symtab.c		\
		table.c			\
		trim.c			\
		zalloc.c		\
		y.tab.c			\
		$(NULL)

PROGRAM = burg

include $(DEPTH)/config/config.mk

include $(DEPTH)/config/rules.mk

#
# Rules to generate y.tab.h and y.tab.c
#
# Redefine this rule because of the warnings. !!!!
$(OBJDIR)/y.tab.$(OBJ_SUFFIX): y.tab.c
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), WINNT)
	$(CC) -Fo$@ -c $(CFLAGS) $<
else
	$(CC) -o $@ -c $(CFLAGS) $<
endif

#
# Extra dependencies.
#
lex.c: y.tab.h

y.tab.c y.tab.h: gram.y
	$(YACC) $(YACC_FLAGS) -d $<

#
# Extra cleaning.
#

clobber:: 
	rm -f y.tab.c y.tab.h

realclean clobber_all::
	rm -f y.tab.c y.tab.h

export:: $(PROGRAM)
