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

!if !defined(VERBOSE)
.SILENT:
!endif
#//------------------------------------------------------------------------
#//
#// This makefile contains all of the rules necessary to build 16 and 32 bit
#// libraries.
#//
#//------------------------------------------------------------------------
!if defined(LIBRARY) && !defined(CONFIG_LIB_INC)
CONFIG_LIB_INC=1

!if "$(MOZ_BITS)" == "16"
#//------------------------------------------------------------------------
#//
#// Rule to build a 16-bit Library
#//
#//------------------------------------------------------------------------
$(LIBRARY):: $(OBJDIR) $(OBJS)
#   //
#   // create response file for the command.  The format is:
#   //     LIBNAME, -+foo.obj -+bar.obj , LISTFILE, NEWLIB
#   //
	rm -f $(CFGFILE)
!if "$(OS)" == "Windows_NT"
        for %%d in ($(OBJS)) do echo -+%%d ^& >> $(CFGFILE)
!else
        for %%d in ($(OBJS)) do echo -+%%d  & >> $(CFGFILE)
!endif
        echo *                                >> $(CFGFILE)
#   //
#   // execute the commands
#   //
    $(RM) $@
    $(AR) $@ @$(CFGFILE)
    $(RANLIB) $@
!else
#//------------------------------------------------------------------------
#//
#// Rule to build a 32-bit Library
#//
#//------------------------------------------------------------------------
$(LIBRARY):: $(OBJDIR) $(OBJS)
    $(RM) $@ 2> NUL
    $(AR) @<<$(CFGFILE)
        -NOLOGO -OUT:$@
        $(OBJS)
<<
    $(RANLIB) $@
!endif

!endif # LIBRARY && ! CONFIG_LIB_INC
