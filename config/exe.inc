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
#// executables.
#//
#//------------------------------------------------------------------------
!if defined(PROGRAM) && !defined(CONFIG_EXE_INC)
CONFIG_EXE_INC=1


!if "$(MOZ_BITS)" == "16"
#//------------------------------------------------------------------------
#//
#// Rule to build a 16-bit executable using the PROGRAM target
#//
#//------------------------------------------------------------------------
$(PROGRAM):: $(OBJDIR) $(OBJS) $(RESFILE) $(DEFFILE) $(MISCDEP)
	echo +++ make: Creating EXE: $@
#   //
#   // create response file for the command.  The format is:
#   //     Object files, Output file, Map file, Libraries, DEF file, RES file
#   //
        echo $(LFLAGS)                       > $(CFGFILE)
        for %%d in ($(OBJS)) do echo %%d  + >> $(CFGFILE)
        echo.                               >> $(CFGFILE)
        echo $(OBJDIR)\$(*B).exe,           >> $(CFGFILE)
        echo $(MAPFILE),                    >> $(CFGFILE)
        echo $(LLIBS) $(OS_LIBS)            >> $(CFGFILE)
!ifdef DEFFILE
        echo $(DEFFILE),                    >> $(CFGFILE)
!else
        echo.                               >> $(CFGFILE)
!endif
!ifdef RESFILE
        echo $(RESFILE),                    >> $(CFGFILE)
!else
        echo.                               >> $(CFGFILE)
!endif
#   //
#   // execute the command
#   //
    $(LD) /ST:8192 @$(CFGFILE)

!else
#//------------------------------------------------------------------------
#//
#// Rule to build a 32-bit executable using the PROGRAM target
#//
#//------------------------------------------------------------------------
$(PROGRAM):: $(OBJDIR) $(OBJS) $(RESFILE) $(DEFFILE) $(MISCDEP)
	echo +++ make: Creating EXE: $@
    $(LD) @<<$(CFGFILE)
        /NOLOGO /OUT:$@ 
		/PDB:$(PDBFILE)
!ifdef DEFFILE
        /DEF:$(DEFFILE)
!endif
!ifdef MAPFILE
		/MAP:$(MAPFILE)
!endif

        $(LFLAGS)
        $(OBJS) 
        $(RESFILE)
        $(LLIBS) $(OS_LIBS)
<<
!endif

!endif # PROGRAM && ! CONFIG_EXE_INC
