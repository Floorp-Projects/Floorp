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