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
$(LIBRARY):: $(OBJDIR) $(OBJS) makefile.win $(DEPTH)\config\lib.inc
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
$(LIBRARY):: $(OBJDIR) $(OBJS) $(SUB_LIBRARIES) makefile.win $(DEPTH)\config\lib.inc
    $(RM) $@ 2> NUL
    $(AR) @<<$(CFGFILE)
        -NOLOGO -OUT:$@
        $(OBJS)
        $(SUB_LIBRARIES)
<<
    $(RANLIB) $@
!endif

!endif # LIBRARY && ! CONFIG_LIB_INC
