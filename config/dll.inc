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
#// DLLs.
#//
#//------------------------------------------------------------------------
!if defined(DLL) && !defined(CONFIG_DLL_INC)
CONFIG_DLL_INC=1


!if "$(MOZ_BITS)" == "16"
#//------------------------------------------------------------------------
#//
#// Rule to build a 16-bit DLL using the DLL target
#//
#//------------------------------------------------------------------------
$(DLL): $(OBJDIR) $(OBJS) $(LLIBS) $(RESFILE) $(DEFFILE) $(MISCDEP) makefile.win  $(DEPTH)\config\dll.inc
	echo +++ make: Creating DLL: $@
#   //
#   // create response file for the command.  The format is:
#   //     Object files, Output file, Map file, Libraries, DEF file, RES file
#   //
        echo $(LFLAGS)                        > $(CFGFILE)
	echo /implib:$*.lib		     >> $(CFGFILE)
        for %%d in ($(OBJS)) do echo %%d  +  >> $(CFGFILE)
        echo.                                >> $(CFGFILE)
        echo $(OBJDIR)\$(*B).dll,            >> $(CFGFILE)
!ifdef MAPFILE
        echo $(MAPFILE),                     >> $(CFGFILE)
!endif
        echo $(LLIBS) $(WIN_LIBS) $(OS_LIBS) >> $(CFGFILE)
!ifdef DEFFILE
        echo $(DEFFILE),                     >> $(CFGFILE)
!else
        echo.                                >> $(CFGFILE)
!endif
!ifdef RESFILE
        echo $(RESFILE),                     >> $(CFGFILE)
!else
        echo.                                >> $(CFGFILE)
!endif
#   //
#   // execute the commands
#   //
    $(LD) @$(CFGFILE)

!else
#//------------------------------------------------------------------------
#//
#// Rule to build a 32-bit DLL using the DLL target
#//
#//------------------------------------------------------------------------
$(DLL): $(OBJDIR) $(OBJS) $(LLIBS) $(RESFILE) $(DEFFILE) $(EXTRA_LIBS_LIST_FILE) $(MISCDEP) makefile.win $(DEPTH)\config\dll.inc
	echo +++ make: Creating DLL: $@
        cat << $(EXTRA_LIBS_LIST_FILE) << >$(CFGFILE)
                /NOLOGO /DLL /OUT:$@ /PDB:$(PDBFILE)
!ifdef DEFFILE
                /DEF:$(DEFFILE)
!endif
!ifdef MAPFILE
                /MAP:$(MAPFILE)
!if defined(MOZ_MAPFILE)
                /MAPINFO:LINES
!endif
!endif

!if exist(win32.order) && !defined(MOZ_DEBUG) && defined(MOZ_COVERAGE)
                /ORDER:@win32.order
!endif
                $(LFLAGS)
                $(OBJS)
                $(RESFILE)
<<NOKEEP
                $(LLIBS) $(WIN_LIBS) $(OS_LIBS)
<<NOKEEP
        $(LD) @$(CFGFILE)
!endif

!endif # DLL && ! CONFIG_DLL_INC
