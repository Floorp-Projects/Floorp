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

#  
# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=rasexit - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to rasexit - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "rasexit - Win32 Release" && "$(CFG)" !=\
 "rasexit - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "Rasexit.mak" CFG="rasexit - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rasexit - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "rasexit - Win32 Debug" (based on "Win32 (x86) Application")
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
# PROP Target_Last_Scanned "rasexit - Win32 Debug"
RSC=rc.exe
MTL=mktyplib.exe
CPP=cl.exe

!IF  "$(CFG)" == "rasexit - Win32 Release"

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

ALL : "$(OUTDIR)\Rasexit.exe" "$(OUTDIR)\rasexit.tlb"

CLEAN : 
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\rasexit.obj"
	-@erase "$(INTDIR)\Rasexit.pch"
	-@erase "$(INTDIR)\rasexit.res"
	-@erase "$(INTDIR)\rasexit.tlb"
	-@erase "$(INTDIR)\rasexitDoc.obj"
	-@erase "$(INTDIR)\rasexitView.obj"
	-@erase "$(INTDIR)\Shutdown.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(OUTDIR)\Rasexit.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(OUTDIR)" :

"$(OUTDIR)\rasexit.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)

.c{$(CPP_OBJS)}.obj:

.c{$(CPP_SBRS)}.sbr:

.cpp{$(CPP_OBJS)}.obj:

.cpp{$(CPP_SBRS)}.sbr:

.cxx{$(CPP_OBJS)}.obj:

.cxx{$(CPP_SBRS)}.sbr:

# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/Rasexit.pch" /Yu"stdafx.h" /Fo"$(INTDIR)/"\
 /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/rasexit.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Rasexit.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 rasapi32.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=rasapi32.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)/Rasexit.pdb" /machine:I386 /out:"$(OUTDIR)/Rasexit.exe" 
LINK32_OBJS= \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\rasexit.obj" \
	"$(INTDIR)\rasexit.res" \
	"$(INTDIR)\rasexitDoc.obj" \
	"$(INTDIR)\rasexitView.obj" \
	"$(INTDIR)\Shutdown.obj" \
	"$(INTDIR)\StdAfx.obj"

"$(OUTDIR)\Rasexit.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "rasexit - Win32 Debug"

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

ALL : "$(OUTDIR)\Rasexit.exe" "$(OUTDIR)\rasexit.tlb"

CLEAN : 
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\rasexit.obj"
	-@erase "$(INTDIR)\Rasexit.pch"
	-@erase "$(INTDIR)\rasexit.res"
	-@erase "$(INTDIR)\rasexit.tlb"
	-@erase "$(INTDIR)\rasexitDoc.obj"
	-@erase "$(INTDIR)\rasexitView.obj"
	-@erase "$(INTDIR)\Shutdown.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\Rasexit.exe"
	-@erase "$(OUTDIR)\Rasexit.ilk"
	-@erase "$(OUTDIR)\Rasexit.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(OUTDIR)" :

"$(OUTDIR)\rasexit.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)

.c{$(CPP_OBJS)}.obj:

.c{$(CPP_SBRS)}.sbr:

.cpp{$(CPP_OBJS)}.obj:

.cpp{$(CPP_SBRS)}.sbr:

.cxx{$(CPP_OBJS)}.obj:

.cxx{$(CPP_SBRS)}.sbr:

# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/Rasexit.pch" /Yu"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/rasexit.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/Rasexit.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 rasapi32.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=rasapi32.lib /nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)/Rasexit.pdb" /debug /machine:I386 /out:"$(OUTDIR)/Rasexit.exe" 
LINK32_OBJS= \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\rasexit.obj" \
	"$(INTDIR)\rasexit.res" \
	"$(INTDIR)\rasexitDoc.obj" \
	"$(INTDIR)\rasexitView.obj" \
	"$(INTDIR)\Shutdown.obj" \
	"$(INTDIR)\StdAfx.obj"

"$(OUTDIR)\Rasexit.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "rasexit - Win32 Release"
# Name "rasexit - Win32 Debug"

!IF  "$(CFG)" == "rasexit - Win32 Release"

!ELSEIF  "$(CFG)" == "rasexit - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\rasexit.cpp
DEP_CPP_RASEX=\
	".\MainFrm.h"\
	".\rasexit.h"\
	".\rasexitDoc.h"\
	".\rasexitView.h"\
	".\stdafx.h"\
	

"$(INTDIR)\rasexit.obj" : $(SOURCE) $(DEP_CPP_RASEX) "$(INTDIR)"\
 "$(INTDIR)\Rasexit.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\rasexit.odl

!IF  "$(CFG)" == "rasexit - Win32 Release"


"$(OUTDIR)\rasexit.tlb" : $(SOURCE) "$(OUTDIR)"
   $(MTL) /nologo /D "NDEBUG" /tlb "$(OUTDIR)/rasexit.tlb" /win32 $(SOURCE)


!ELSEIF  "$(CFG)" == "rasexit - Win32 Debug"


"$(OUTDIR)\rasexit.tlb" : $(SOURCE) "$(OUTDIR)"
   $(MTL) /nologo /D "_DEBUG" /tlb "$(OUTDIR)/rasexit.tlb" /win32 $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\StdAfx.cpp
DEP_CPP_STDAF=\
	".\stdafx.h"\
	

!IF  "$(CFG)" == "rasexit - Win32 Release"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/Rasexit.pch" /Yc"stdafx.h" /Fo"$(INTDIR)/"\
 /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\Rasexit.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ELSEIF  "$(CFG)" == "rasexit - Win32 Debug"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/Rasexit.pch" /Yc"stdafx.h"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\Rasexit.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\MainFrm.cpp
DEP_CPP_MAINF=\
	".\MainFrm.h"\
	".\rasexit.h"\
	".\stdafx.h"\
	

"$(INTDIR)\MainFrm.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"\
 "$(INTDIR)\Rasexit.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\rasexitDoc.cpp
DEP_CPP_RASEXI=\
	".\rasexit.h"\
	".\rasexitDoc.h"\
	".\stdafx.h"\
	

"$(INTDIR)\rasexitDoc.obj" : $(SOURCE) $(DEP_CPP_RASEXI) "$(INTDIR)"\
 "$(INTDIR)\Rasexit.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\rasexitView.cpp
DEP_CPP_RASEXIT=\
	".\rasexit.h"\
	".\rasexitDoc.h"\
	".\rasexitView.h"\
	".\stdafx.h"\
	

"$(INTDIR)\rasexitView.obj" : $(SOURCE) $(DEP_CPP_RASEXIT) "$(INTDIR)"\
 "$(INTDIR)\Rasexit.pch"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\rasexit.rc

!IF  "$(CFG)" == "rasexit - Win32 Release"

DEP_RSC_RASEXIT_=\
	".\Res\Rasexit.ico"\
	".\Res\Rasexit.rc2"\
	".\Res\rasexitDoc.ico"\
	".\Res\Toolbar.bmp"\
	

"$(INTDIR)\rasexit.res" : $(SOURCE) $(DEP_RSC_RASEXIT_) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/rasexit.res" /i "Release" /d "NDEBUG" /d\
 "_AFXDLL" $(SOURCE)


!ELSEIF  "$(CFG)" == "rasexit - Win32 Debug"

DEP_RSC_RASEXIT_=\
	".\Res\Rasexit.ico"\
	".\Res\Rasexit.rc2"\
	".\Res\rasexitDoc.ico"\
	".\Res\Toolbar.bmp"\
	

"$(INTDIR)\rasexit.res" : $(SOURCE) $(DEP_RSC_RASEXIT_) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/rasexit.res" /i "Debug" /d "_DEBUG" /d\
 "_AFXDLL" $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\Shutdown.cpp
DEP_CPP_SHUTD=\
	".\rasexit.h"\
	".\Shutdown.h"\
	".\stdafx.h"\
	

"$(INTDIR)\Shutdown.obj" : $(SOURCE) $(DEP_CPP_SHUTD) "$(INTDIR)"\
 "$(INTDIR)\Rasexit.pch"


# End Source File
# End Target
# End Project
################################################################################
