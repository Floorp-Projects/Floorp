# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

!if !defined(VERBOSE)
.SILENT:
!endif

#//------------------------------------------------------------------------
#//
#// This makefile contains all of the rules necessary to build 16 and 32 bit
#// object files.
#//
#//------------------------------------------------------------------------
!if !defined(CONFIG_OBJ_INC)
CONFIG_OBJ_INC=1

#// objs must be rebuilt if any of the makefiles change, otherwise compiler
#// settings and the like will be missed.
#// -- the two space definition worries me, fragile! --
!if "$(OBJS)" != "  "
$(OBJS): makefile.win \
         $(DEPTH)\config\rules.mak \
         $(DEPTH)\config\config.mak \
         $(DEPTH)\config\win32 \
         $(DEPTH)\config\obj.inc
!endif

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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).c
<<
    $(LD) @<<$(CFGFILE)
        $(LFLAGS)
        $(OBJDIR)\$(*B).obj,
        $(OBJDIR)\$(*B).exe,
        $(MAPFILE),
        $(LLIBS) $(WIN_LIBS) $(OS_LIBS),
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
        $(LLIBS)
        $(OS_LIBS)
        -Fd$(PDBFILE)
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
        -Fe.\$(OBJDIR)\
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).c
<<KEEP

.cpp{.\$(OBJDIR)\}.exe:
    $(CC) @<<$(CFGFILE)
        $(CFLAGS)
        $(LCFLAGS)
        $(LINCS)
        $(LINCS_1)
        $(INCS)
        $(LLIBS)
        $(WIN_LIBS)
        $(OS_LIBS)
        /link $(LFLAGS)
        -Fd$(PDBFILE)
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
        -Fe.\$(OBJDIR)\
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).cpp
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
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
!if defined(MOZ_MAPINFO)
        -Fm$(MAPFILE)
!endif
!if defined(MOZ_CODINFO)
        -Fa$(OBJDIR)/$(*B).cod 
        -FAsc
!endif
        -Fo.\$(OBJDIR)\
        $(CURDIR)$(*B).cpp
<<KEEP


!endif # CONFIG_OBJ_INC
