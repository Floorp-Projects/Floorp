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
CFG=nabapi32 - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to nabapi32 - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "nabapi32 - Win32 Release" && "$(CFG)" !=\
 "nabapi32 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "nabapi32.mak" CFG="nabapi32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "nabapi32 - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "nabapi32 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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
# PROP Target_Last_Scanned "nabapi32 - Win32 Debug"
CPP=cl.exe
MTL=mktyplib.exe
RSC=rc.exe

!IF  "$(CFG)" == "nabapi32 - Win32 Release"

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

ALL : "$(OUTDIR)\nabapi32.dll"

CLEAN : 
	-@erase "$(INTDIR)\maindll.obj"
	-@erase "$(INTDIR)\mapismem.obj"
	-@erase "$(INTDIR)\nabapi.obj"
	-@erase "$(INTDIR)\nabapi32.res"
	-@erase "$(INTDIR)\nabipc.obj"
	-@erase "$(INTDIR)\nabmem.obj"
	-@erase "$(INTDIR)\nabutl.obj"
	-@erase "$(INTDIR)\nsstrseq.obj"
	-@erase "$(INTDIR)\trace.obj"
	-@erase "$(INTDIR)\xpapi.obj"
	-@erase "$(OUTDIR)\nabapi32.dll"
	-@erase "$(OUTDIR)\nabapi32.exp"
	-@erase "$(OUTDIR)\nabapi32.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "\ns\cmd\winfe" /I "\ns\cmd\winfe\nabapi" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "NABAPIDLL" /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "\ns\cmd\winfe" /I "\ns\cmd\winfe\nabapi"\
 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "NABAPIDLL" /Fp"$(INTDIR)/nabapi32.pch"\
 /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\x86Rel/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nabapi32.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/nabapi32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/nabapi32.pdb" /machine:I386 /def:".\nabapi32.def"\
 /out:"$(OUTDIR)/nabapi32.dll" /implib:"$(OUTDIR)/nabapi32.lib" 
DEF_FILE= \
	".\nabapi32.def"
LINK32_OBJS= \
	"$(INTDIR)\maindll.obj" \
	"$(INTDIR)\mapismem.obj" \
	"$(INTDIR)\nabapi.obj" \
	"$(INTDIR)\nabapi32.res" \
	"$(INTDIR)\nabipc.obj" \
	"$(INTDIR)\nabmem.obj" \
	"$(INTDIR)\nabutl.obj" \
	"$(INTDIR)\nsstrseq.obj" \
	"$(INTDIR)\trace.obj" \
	"$(INTDIR)\xpapi.obj"

"$(OUTDIR)\nabapi32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "nabapi32 - Win32 Debug"

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

ALL : "$(OUTDIR)\nabapi32.dll"

CLEAN : 
	-@erase "$(INTDIR)\maindll.obj"
	-@erase "$(INTDIR)\mapismem.obj"
	-@erase "$(INTDIR)\nabapi.obj"
	-@erase "$(INTDIR)\nabapi32.res"
	-@erase "$(INTDIR)\nabipc.obj"
	-@erase "$(INTDIR)\nabmem.obj"
	-@erase "$(INTDIR)\nabutl.obj"
	-@erase "$(INTDIR)\nsstrseq.obj"
	-@erase "$(INTDIR)\trace.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(INTDIR)\xpapi.obj"
	-@erase "$(OUTDIR)\nabapi32.dll"
	-@erase "$(OUTDIR)\nabapi32.exp"
	-@erase "$(OUTDIR)\nabapi32.ilk"
	-@erase "$(OUTDIR)\nabapi32.lib"
	-@erase "$(OUTDIR)\nabapi32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "\ns\cmd\winfe" /I "\ns\cmd\winfe\nabapi" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "NABAPIDLL" /YX /c
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "\ns\cmd\winfe" /I\
 "\ns\cmd\winfe\nabapi" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "NABAPIDLL"\
 /Fp"$(INTDIR)/nabapi32.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\x86Dbg/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/nabapi32.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/nabapi32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/nabapi32.pdb" /debug /machine:I386 /def:".\nabapi32.def"\
 /out:"$(OUTDIR)/nabapi32.dll" /implib:"$(OUTDIR)/nabapi32.lib" 
DEF_FILE= \
	".\nabapi32.def"
LINK32_OBJS= \
	"$(INTDIR)\maindll.obj" \
	"$(INTDIR)\mapismem.obj" \
	"$(INTDIR)\nabapi.obj" \
	"$(INTDIR)\nabapi32.res" \
	"$(INTDIR)\nabipc.obj" \
	"$(INTDIR)\nabmem.obj" \
	"$(INTDIR)\nabutl.obj" \
	"$(INTDIR)\nsstrseq.obj" \
	"$(INTDIR)\trace.obj" \
	"$(INTDIR)\xpapi.obj"

"$(OUTDIR)\nabapi32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "nabapi32 - Win32 Release"
# Name "nabapi32 - Win32 Debug"

!IF  "$(CFG)" == "nabapi32 - Win32 Release"

!ELSEIF  "$(CFG)" == "nabapi32 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\maindll.cpp

"$(INTDIR)\maindll.obj" : $(SOURCE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabapi.cpp

!IF  "$(CFG)" == "nabapi32 - Win32 Release"

DEP_CPP_NABAP=\
	".\nabipc.h"\
	".\nabmem.h"\
	".\nabutl.h"\
	".\port.h"\
	".\trace.h"\
	".\xpapi.h"\
	"\ns\cmd\winfe\abapi.h"\
	"\ns\cmd\winfe\mapismem.h"\
	"\ns\cmd\winfe\nabapi.h"\
	"\ns\cmd\winfe\nsstrseq.h"\
	
NODEP_CPP_NABAP=\
	"..\abcom.h"\
	"..\msgcom.h"\
	

"$(INTDIR)\nabapi.obj" : $(SOURCE) $(DEP_CPP_NABAP) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "nabapi32 - Win32 Debug"

DEP_CPP_NABAP=\
	".\nabipc.h"\
	".\nabmem.h"\
	".\nabutl.h"\
	".\port.h"\
	".\trace.h"\
	".\xpapi.h"\
	"\ns\cmd\winfe\abapi.h"\
	"\ns\cmd\winfe\mapismem.h"\
	"\ns\cmd\winfe\nabapi.h"\
	"\ns\cmd\winfe\nsstrseq.h"\
	
NODEP_CPP_NABAP=\
	"..\abcom.h"\
	"..\msgcom.h"\
	

"$(INTDIR)\nabapi.obj" : $(SOURCE) $(DEP_CPP_NABAP) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabapi32.rc
DEP_RSC_NABAPI=\
	".\resource.h"\
	

"$(INTDIR)\nabapi32.res" : $(SOURCE) $(DEP_RSC_NABAPI) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabipc.cpp
DEP_CPP_NABIP=\
	".\nabipc.h"\
	".\port.h"\
	".\trace.h"\
	"\ns\cmd\winfe\abapi.h"\
	"\ns\cmd\winfe\mapismem.h"\
	"\ns\cmd\winfe\nabapi.h"\
	
NODEP_CPP_NABIP=\
	"..\abcom.h"\
	"..\msgcom.h"\
	

"$(INTDIR)\nabipc.obj" : $(SOURCE) $(DEP_CPP_NABIP) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabmem.cpp
DEP_CPP_NABME=\
	".\nabmem.h"\
	".\nabutl.h"\
	".\trace.h"\
	".\xpapi.h"\
	"\ns\cmd\winfe\abapi.h"\
	"\ns\cmd\winfe\nabapi.h"\
	"\ns\cmd\winfe\nsstrseq.h"\
	
NODEP_CPP_NABME=\
	"..\abcom.h"\
	"..\msgcom.h"\
	

"$(INTDIR)\nabmem.obj" : $(SOURCE) $(DEP_CPP_NABME) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabutl.cpp
DEP_CPP_NABUT=\
	".\nabipc.h"\
	".\nabutl.h"\
	".\port.h"\
	".\trace.h"\
	".\xpapi.h"\
	"\ns\cmd\winfe\abapi.h"\
	"\ns\cmd\winfe\nabapi.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	
NODEP_CPP_NABUT=\
	"..\abcom.h"\
	"..\msgcom.h"\
	

"$(INTDIR)\nabutl.obj" : $(SOURCE) $(DEP_CPP_NABUT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\trace.cpp
DEP_CPP_TRACE=\
	".\trace.h"\
	

"$(INTDIR)\trace.obj" : $(SOURCE) $(DEP_CPP_TRACE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\xpapi.cpp
DEP_CPP_XPAPI=\
	".\nabutl.h"\
	".\xpapi.h"\
	"\ns\cmd\winfe\nabapi.h"\
	

"$(INTDIR)\xpapi.obj" : $(SOURCE) $(DEP_CPP_XPAPI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\abapi.h

!IF  "$(CFG)" == "nabapi32 - Win32 Release"

!ELSEIF  "$(CFG)" == "nabapi32 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\nabapi.h

!IF  "$(CFG)" == "nabapi32 - Win32 Release"

!ELSEIF  "$(CFG)" == "nabapi32 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\mapismem.cpp
DEP_CPP_MAPIS=\
	"\ns\cmd\winfe\mapismem.h"\
	

"$(INTDIR)\mapismem.obj" : $(SOURCE) $(DEP_CPP_MAPIS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\nabapi32.def

!IF  "$(CFG)" == "nabapi32 - Win32 Release"

!ELSEIF  "$(CFG)" == "nabapi32 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\ns\cmd\winfe\nsstrseq.cpp
DEP_CPP_NSSTR=\
	"\ns\cmd\winfe\nsstrseq.h"\
	

"$(INTDIR)\nsstrseq.obj" : $(SOURCE) $(DEP_CPP_NSSTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
