# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

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
# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=mapi32 - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to mapi32 - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "mapi32 - Win32 Release" && "$(CFG)" != "mapi32 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "mapi32.mak" CFG="mapi32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mapi32 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mapi32 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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
# PROP Target_Last_Scanned "mapi32 - Win32 Debug"
CPP=cl.exe
MTL=mktyplib.exe
RSC=rc.exe

!IF  "$(CFG)" == "mapi32 - Win32 Release"

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

ALL : "$(OUTDIR)\mapi32.dll"

CLEAN : 
	-@erase "$(INTDIR)\maindll.obj"
	-@erase "$(INTDIR)\mapi32.obj"
	-@erase "$(INTDIR)\mapi32.res"
	-@erase "$(INTDIR)\mapiipc.obj"
	-@erase "$(INTDIR)\mapimem.obj"
	-@erase "$(INTDIR)\mapismem.obj"
	-@erase "$(INTDIR)\mapiutl.obj"
	-@erase "$(INTDIR)\nsstrseq.obj"
	-@erase "$(INTDIR)\trace.obj"
	-@erase "$(INTDIR)\xpapi.obj"
	-@erase "$(OUTDIR)\mapi32.dll"
	-@erase "$(OUTDIR)\mapi32.exp"
	-@erase "$(OUTDIR)\mapi32.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /Gz /MT /W3 /GX /O2 /I ".." /I ".\mapi" /I "..\mapi" /D "NDEBUG" /D "NSCPMAPIDLL" /D "WIN32" /D "_WINDOWS" /D "STRICT" /YX /c
CPP_PROJ=/nologo /Gz /MT /W3 /GX /O2 /I ".." /I ".\mapi" /I "..\mapi" /D\
 "NDEBUG" /D "NSCPMAPIDLL" /D "WIN32" /D "_WINDOWS" /D "STRICT"\
 /Fp"$(INTDIR)/mapi32.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\x86Rel/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/mapi32.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/mapi32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)/mapi32.pdb"\
 /machine:I386 /def:".\mapi32.def" /out:"$(OUTDIR)/mapi32.dll"\
 /implib:"$(OUTDIR)/mapi32.lib" 
DEF_FILE= \
	".\mapi32.def"
LINK32_OBJS= \
	"$(INTDIR)\maindll.obj" \
	"$(INTDIR)\mapi32.obj" \
	"$(INTDIR)\mapi32.res" \
	"$(INTDIR)\mapiipc.obj" \
	"$(INTDIR)\mapimem.obj" \
	"$(INTDIR)\mapismem.obj" \
	"$(INTDIR)\mapiutl.obj" \
	"$(INTDIR)\nsstrseq.obj" \
	"$(INTDIR)\trace.obj" \
	"$(INTDIR)\xpapi.obj"

"$(OUTDIR)\mapi32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mapi32 - Win32 Debug"

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

ALL : "$(OUTDIR)\mapi32.dll"

CLEAN : 
	-@erase "$(INTDIR)\maindll.obj"
	-@erase "$(INTDIR)\mapi32.obj"
	-@erase "$(INTDIR)\mapi32.res"
	-@erase "$(INTDIR)\mapiipc.obj"
	-@erase "$(INTDIR)\mapimem.obj"
	-@erase "$(INTDIR)\mapismem.obj"
	-@erase "$(INTDIR)\mapiutl.obj"
	-@erase "$(INTDIR)\nsstrseq.obj"
	-@erase "$(INTDIR)\trace.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(INTDIR)\xpapi.obj"
	-@erase "$(OUTDIR)\mapi32.dll"
	-@erase "$(OUTDIR)\mapi32.exp"
	-@erase "$(OUTDIR)\mapi32.ilk"
	-@erase "$(OUTDIR)\mapi32.lib"
	-@erase "$(OUTDIR)\mapi32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /Gz /MTd /W3 /Gm /GX /Zi /Od /I ".." /I ".\mapi" /I "..\mapi" /D "_DEBUG" /D "NSCPMAPIDLL" /D "WIN32" /D "_WINDOWS" /D "STRICT" /YX /c
CPP_PROJ=/nologo /Gz /MTd /W3 /Gm /GX /Zi /Od /I ".." /I ".\mapi" /I "..\mapi"\
 /D "_DEBUG" /D "NSCPMAPIDLL" /D "WIN32" /D "_WINDOWS" /D "STRICT"\
 /Fp"$(INTDIR)/mapi32.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\x86Dbg/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/mapi32.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/mapi32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)/mapi32.pdb" /debug\
 /machine:I386 /def:".\mapi32.def" /out:"$(OUTDIR)/mapi32.dll"\
 /implib:"$(OUTDIR)/mapi32.lib" 
DEF_FILE= \
	".\mapi32.def"
LINK32_OBJS= \
	"$(INTDIR)\maindll.obj" \
	"$(INTDIR)\mapi32.obj" \
	"$(INTDIR)\mapi32.res" \
	"$(INTDIR)\mapiipc.obj" \
	"$(INTDIR)\mapimem.obj" \
	"$(INTDIR)\mapismem.obj" \
	"$(INTDIR)\mapiutl.obj" \
	"$(INTDIR)\nsstrseq.obj" \
	"$(INTDIR)\trace.obj" \
	"$(INTDIR)\xpapi.obj"

"$(OUTDIR)\mapi32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "mapi32 - Win32 Release"
# Name "mapi32 - Win32 Debug"

!IF  "$(CFG)" == "mapi32 - Win32 Release"

!ELSEIF  "$(CFG)" == "mapi32 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\mapi32.cpp
DEP_CPP_MAPI3=\
	"..\mapi\mapiutl.h"\
	"..\mapi\trace.h"\
	"..\mapismem.h"\
	"..\nscpmapi.h"\
	"..\nsstrseq.h"\
	".\mapiipc.h"\
	".\mapimem.h"\
	".\port.h"\
	".\xpapi.h"\
	

"$(INTDIR)\mapi32.obj" : $(SOURCE) $(DEP_CPP_MAPI3) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\mapi32.def

!IF  "$(CFG)" == "mapi32 - Win32 Release"

!ELSEIF  "$(CFG)" == "mapi32 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\trace.cpp
DEP_CPP_TRACE=\
	"..\mapi\trace.h"\
	

"$(INTDIR)\trace.obj" : $(SOURCE) $(DEP_CPP_TRACE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\mapi32.rc
DEP_RSC_MAPI32=\
	".\resource.h"\
	

"$(INTDIR)\mapi32.res" : $(SOURCE) $(DEP_RSC_MAPI32) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\mapiipc.cpp
DEP_CPP_MAPII=\
	"..\mapi\trace.h"\
	"..\mapismem.h"\
	"..\nscpmapi.h"\
	".\mapiipc.h"\
	".\port.h"\
	

"$(INTDIR)\mapiipc.obj" : $(SOURCE) $(DEP_CPP_MAPII) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\mapimem.cpp
DEP_CPP_MAPIM=\
	"..\mapi\mapiutl.h"\
	"..\mapi\trace.h"\
	"..\nscpmapi.h"\
	"..\nsstrseq.h"\
	".\mapimem.h"\
	".\xpapi.h"\
	

"$(INTDIR)\mapimem.obj" : $(SOURCE) $(DEP_CPP_MAPIM) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\xpapi.cpp
DEP_CPP_XPAPI=\
	"..\mapi\mapiutl.h"\
	"..\mapi\trace.h"\
	".\xpapi.h"\
	

"$(INTDIR)\xpapi.obj" : $(SOURCE) $(DEP_CPP_XPAPI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\nsstrseq.cpp
DEP_CPP_NSSTR=\
	"..\nsstrseq.h"\
	

"$(INTDIR)\nsstrseq.obj" : $(SOURCE) $(DEP_CPP_NSSTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\mapismem.cpp
DEP_CPP_MAPIS=\
	"..\mapismem.h"\
	

"$(INTDIR)\mapismem.obj" : $(SOURCE) $(DEP_CPP_MAPIS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\mapiutl.cpp
DEP_CPP_MAPIU=\
	"..\mapi\mapiutl.h"\
	"..\mapi\trace.h"\
	"..\nscpmapi.h"\
	".\mapiipc.h"\
	".\port.h"\
	".\xpapi.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mapiutl.obj" : $(SOURCE) $(DEP_CPP_MAPIU) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\maindll.cpp

"$(INTDIR)\maindll.obj" : $(SOURCE) "$(INTDIR)"


# End Source File
# End Target
# End Project
################################################################################
