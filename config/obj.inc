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

## Include support for MOZ_LITE/MOZ_MEDIUM
#include <$(DEPTH)/config/liteness.mak>

RCFLAGS=$(RCFLAGS) $(MOZ_LITENESS_FLAGS)

#//------------------------------------------------------------------------
#//
#// This makefile contains all of the rules necessary to build 16 and 32 bit
#// object files.
#//
#//------------------------------------------------------------------------
!if !defined(CONFIG_OBJ_INC)
CONFIG_OBJ_INC=1

#//------------------------------------------------------------------------
#//
#// Rules for compiling 16/32 bit object files from either c or c++ source
#//
#//------------------------------------------------------------------------
.c.obj:
    @$(CC) @<<$(CFGFILE)
        -nologo -c $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        $(CURDIR)$(*B).c
<<KEEP

.c{.\$(OBJDIR)\}.obj:
    @$(CC) @<<$(CFGFILE)
        -nologo -c $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).c
<<KEEP

{.\_jmc\}.c{.\$(OBJDIR)\}.obj:
    @$(CC) @<<$(CFGFILE)
        -nologo -c $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)_jmc\$(*B).c
<<KEEP

.cpp.obj:
    @$(CC) @<<$(CFGFILE)
        -nologo -c $(OPTIMIZER)
        $(CFLAGS)
        $(LCFLAGS)
        $(LINCS)
        $(LINCS_1)
        $(INCS)
        -Fd$(PDBFILE)
        $(CURDIR)$(*B).cpp
<<KEEP

.cpp{.\$(OBJDIR)\}.obj:
    @$(CC) @<<$(CFGFILE)
        -nologo -c $(OPTIMIZER)
        $(CFLAGS)
        $(LCFLAGS)
        $(LINCS)
        $(LINCS_1)
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).cpp
<<KEEP


{.\_jmc\}.cpp{.\$(OBJDIR)\}.obj:
    @$(CC) @<<$(CFGFILE)
        -nologo -c $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)_jmc\$(*B).cpp
<<KEEP
#//------------------------------------------------------------------------
#//
#// Rule for compiling resource files
#//
#//------------------------------------------------------------------------
.rc{.\$(OBJDIR)\}.res:
#   //
#   // execute the command
#   //
    echo Creating Resource file: $*.res
    $(RC) $(RCFLAGS) -r -Fo.\$(OBJDIR)\$(*B).res $(*B).rc


!if "$(MOZ_BITS)" == "16"
#//------------------------------------------------------------------------
#//
#// Rule for building simple 16 bit executables
#//
#//------------------------------------------------------------------------
.c{.\$(OBJDIR)\}.exe:
    $(CC) @<<$(CFGFILE)
        -c 
        $(OPTIMIZER)
        $(CFLAGS)
        $(LCFLAGS)
        $(LINCS)
        $(LINCS_1)
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).c
<<
    $(LD) @<<$(CFGFILE)
        $(LFLAGS)
        $(OBJDIR)\$(*B).obj,
        $(OBJDIR)\$(*B).exe,
        $(MAPFILE),
        $(LLIBS) $(OS_LIBS),
        $(DEFFILE),
        $(RESFILE),
<<

!else
#//------------------------------------------------------------------------
#//
#// Rule for building simple 32 bit executables
#//
#//------------------------------------------------------------------------
.c{.\$(OBJDIR)\}.exe:
    $(CC) @<<$(CFGFILE)
        $(CFLAGS)
        $(LCFLAGS)
        $(LINCS)
        $(LINCS_1)
        $(INCS)
        -Fd$(PDBFILE)
        -Fe.\$(OBJDIR)\
        $(CURDIR)$(*B).c
<<KEEP
!endif


#//------------------------------------------------------------------------
#//
#// Rule for creating .i file containing c preprocessor output
#//
#//------------------------------------------------------------------------
.c.i:
    @$(CC) @<<$(CFGFILE)
        /P -c 
        $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        $(CURDIR)$(*B).c
<<KEEP

.c{.\$(OBJDIR)\}.i:
    @$(CC) @<<$(CFGFILE)
        /P -c 
        $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).c
<<KEEP

.cpp.i:
    @$(CC) @<<$(CFGFILE)
        /P -c 
        $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        $(CURDIR)$(*B).cpp
<<KEEP

.cpp{.\$(OBJDIR)\}.i:
    @$(CC) @<<$(CFGFILE)
        /P -c 
        $(OPTIMIZER)
        $(CFLAGS) 
        $(LCFLAGS)
        $(LINCS) 
        $(LINCS_1) 
        $(INCS)
        -Fd$(PDBFILE)
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).cpp
<<KEEP


!endif # CONFIG_OBJ_INC
