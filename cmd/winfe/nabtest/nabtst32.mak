# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
!IF "$(CFG)" == ""
CFG=nabtst32 - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to nabtst32 - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "nabtst32 - Win32 Release" && "$(CFG)" !=\
 "nabtst32 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "nabtst32.mak" CFG="nabtst32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "nabtst32 - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "nabtst32 - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "nabtst32 - Win32 Debug"
CPP=cl.exe
MTL=mktyplib.exe
RSC=rc.exe

!IF  "$(CFG)" == "nabtst32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "x86Rel"
# PROP Intermediate_Dir "x86Rel"
# PROP Target_Dir ""
OUTDIR=.\x86Rel
INTDIR=.\x86Rel

ALL : "$(OUTDIR)\nabtst32.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldifdiag.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\nabproc.obj"
	-@erase "$(INTDIR)\nabtst32.res"
	-@erase "$(OUTDIR)\nabtst32.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /Gz /W3 /GX /O2 /I "\ns\cmd\winfe" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /Gz /ML /W3 /GX /O2 /I "\ns\cmd\winfe" /D "WIN32" /D "NDEBUG"\
 /D "_WINDOWS" /Fp"$(INTDIR)/nabtst32.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\x86Rel/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nabtst32.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/nabtst32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)/nabtst32.pdb" /machine:I386 /out:"$(OUTDIR)/nabtst32.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ldifdiag.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\nabproc.obj" \
	"$(INTDIR)\nabtst32.res"

"$(OUTDIR)\nabtst32.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "nabtst32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "x86Dbg"
# PROP Intermediate_Dir "x86Dbg"
# PROP Target_Dir ""
OUTDIR=.\x86Dbg
INTDIR=.\x86Dbg

ALL : "$(OUTDIR)\nabtst32.exe"

CLEAN : 
	-@erase "$(INTDIR)\ldifdiag.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\nabproc.obj"
	-@erase "$(INTDIR)\nabtst32.res"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\nabtst32.exe"
	-@erase "$(OUTDIR)\nabtst32.ilk"
	-@erase "$(OUTDIR)\nabtst32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /Gz /W3 /Gm /GX /Zi /Od /I "\ns\cmd\winfe" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /Gz /MLd /W3 /Gm /GX /Zi /Od /I "\ns\cmd\winfe" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)/nabtst32.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\x86Dbg/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nabtst32.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/nabtst32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)/nabtst32.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/nabtst32.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ldifdiag.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\nabproc.obj" \
	"$(INTDIR)\nabtst32.res"

"$(OUTDIR)\nabtst32.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "nabtst32 - Win32 Release"
# Name "nabtst32 - Win32 Debug"

!IF  "$(CFG)" == "nabtst32 - Win32 Release"

!ELSEIF  "$(CFG)" == "nabtst32 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\main.cpp
DEP_CPP_MAIN_=\
	".\port.h"\
	"\ns\cmd\winfe\nabapi.h"\
	

"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabproc.cpp
DEP_CPP_NABPR=\
	".\port.h"\
	"\ns\cmd\winfe\nabapi.h"\
	

"$(INTDIR)\nabproc.obj" : $(SOURCE) $(DEP_CPP_NABPR) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabtst32.rc
DEP_RSC_NABTS=\
	".\nscicon.ico"\
	

"$(INTDIR)\nabtst32.res" : $(SOURCE) $(DEP_RSC_NABTS) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\nabapi.h

!IF  "$(CFG)" == "nabtst32 - Win32 Release"

!ELSEIF  "$(CFG)" == "nabtst32 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ldifdiag.cpp
DEP_CPP_LDIFD=\
	".\port.h"\
	

"$(INTDIR)\ldifdiag.obj" : $(SOURCE) $(DEP_CPP_LDIFD) "$(INTDIR)"


# End Source File
# End Target
# End Project
################################################################################
