# -*- Mode: Makefile -*-
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


# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=muc - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to muc - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "muc - Win32 Release" && "$(CFG)" != "muc - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "Muc.mak" CFG="muc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "muc - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "muc - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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
# PROP Target_Last_Scanned "muc - Win32 Debug"
CPP=cl.exe
RSC=rc.exe
MTL=mktyplib.exe

!IF  "$(CFG)" == "muc - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\Muc.dll"

CLEAN : 
	-@erase "$(INTDIR)\dialshr.obj"
	-@erase "$(INTDIR)\MUC.OBJ"
	-@erase "$(INTDIR)\ras95.obj"
	-@erase "$(INTDIR)\rasnt40.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(OUTDIR)\Muc.dll"
	-@erase "$(OUTDIR)\Muc.exp"
	-@erase "$(OUTDIR)\Muc.lib"
	-@erase "$(OUTDIR)\Muc.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /D WINVER=0x401 /YX /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /D WINVER=0x401\
 /Fp"$(INTDIR)/Muc.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_USRDLL" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Muc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 /nologo /subsystem:windows /dll /map /machine:I386 /export:PEPluginFunc=_PEPluginFunc@12
LINK32_FLAGS=/nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/Muc.pdb" /map:"$(INTDIR)/Muc.map" /machine:I386\
 /out:"$(OUTDIR)/Muc.dll" /implib:"$(OUTDIR)/Muc.lib"\
 /export:PEPluginFunc=_PEPluginFunc@12 
LINK32_OBJS= \
	"$(INTDIR)\dialshr.obj" \
	"$(INTDIR)\MUC.OBJ" \
	"$(INTDIR)\ras95.obj" \
	"$(INTDIR)\rasnt40.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"E:\mstools\lib\TAPI32.LIB"

"$(OUTDIR)\Muc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "muc - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\Muc.dll"

CLEAN : 
	-@erase "$(INTDIR)\dialshr.obj"
	-@erase "$(INTDIR)\MUC.OBJ"
	-@erase "$(INTDIR)\ras95.obj"
	-@erase "$(INTDIR)\rasnt40.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\Muc.dll"
	-@erase "$(OUTDIR)\Muc.exp"
	-@erase "$(OUTDIR)\Muc.ilk"
	-@erase "$(OUTDIR)\Muc.lib"
	-@erase "$(OUTDIR)\Muc.map"
	-@erase "$(OUTDIR)\Muc.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /D WINVER=0x401 /YX /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS"\
 /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /D WINVER=0x401\
 /Fp"$(INTDIR)/Muc.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_USRDLL" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Muc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 /nologo /subsystem:windows /dll /map /debug /machine:I386 /export:PEPluginFunc=_PEPluginFunc@12
LINK32_FLAGS=/nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/Muc.pdb" /map:"$(INTDIR)/Muc.map" /debug /machine:I386\
 /out:"$(OUTDIR)/Muc.dll" /implib:"$(OUTDIR)/Muc.lib"\
 /export:PEPluginFunc=_PEPluginFunc@12 
LINK32_OBJS= \
	"$(INTDIR)\dialshr.obj" \
	"$(INTDIR)\MUC.OBJ" \
	"$(INTDIR)\ras95.obj" \
	"$(INTDIR)\rasnt40.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"E:\mstools\lib\TAPI32.LIB"

"$(OUTDIR)\Muc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "muc - Win32 Release"
# Name "muc - Win32 Debug"

!IF  "$(CFG)" == "muc - Win32 Release"

!ELSEIF  "$(CFG)" == "muc - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\ReadMe.txt

!IF  "$(CFG)" == "muc - Win32 Release"

!ELSEIF  "$(CFG)" == "muc - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\src\StdAfx.cpp
DEP_CPP_STDAF=\
	"..\src\stdafx.h"\
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=..\src\MUC.CPP
DEP_CPP_MUC_C=\
	"..\src\dialshr.h"\
	"..\src\muc.h"\
	"..\src\ras16.h"\
	"..\src\stdafx.h"\
	

"$(INTDIR)\MUC.OBJ" : $(SOURCE) $(DEP_CPP_MUC_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=..\src\ras95.cpp
DEP_CPP_RAS95=\
	"..\src\stdafx.h"\
	

"$(INTDIR)\ras95.obj" : $(SOURCE) $(DEP_CPP_RAS95) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=..\src\rasnt40.cpp
DEP_CPP_RASNT=\
	"..\src\stdafx.h"\
	

"$(INTDIR)\rasnt40.obj" : $(SOURCE) $(DEP_CPP_RASNT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=..\src\dialshr.cpp
DEP_CPP_DIALS=\
	"..\src\dialshr.h"\
	"..\src\ras16.h"\
	"..\src\stdafx.h"\
	

"$(INTDIR)\dialshr.obj" : $(SOURCE) $(DEP_CPP_DIALS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=E:\mstools\lib\TAPI32.LIB

!IF  "$(CFG)" == "muc - Win32 Release"

!ELSEIF  "$(CFG)" == "muc - Win32 Debug"

!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
