# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=WizardMachine - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to WizardMachine - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "WizardMachine - Win32 Release" && "$(CFG)" !=\
 "WizardMachine - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "WizardMachine.mak" CFG="WizardMachine - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "WizardMachine - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "WizardMachine - Win32 Debug" (based on "Win32 (x86) Application")
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
# PROP Target_Last_Scanned "WizardMachine - Win32 Debug"
CPP=cl.exe
MTL=mktyplib.exe
RSC=rc.exe

!IF  "$(CFG)" == "WizardMachine - Win32 Release"

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

ALL : "$(OUTDIR)\WizardMachine.exe"

CLEAN : 
	-@erase "$(INTDIR)\ImageDialog.obj"
	-@erase "$(INTDIR)\NavText.obj"
	-@erase "$(INTDIR)\NewConfigDialog.obj"
	-@erase "$(INTDIR)\NewDialog.obj"
	-@erase "$(INTDIR)\ProgDlgThread.obj"
	-@erase "$(INTDIR)\ProgressDialog.obj"
	-@erase "$(INTDIR)\PropSheet.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\WizardMachine.obj"
	-@erase "$(INTDIR)\WizardMachine.pch"
	-@erase "$(INTDIR)\WizardMachine.res"
	-@erase "$(INTDIR)\WizardMachineDlg.obj"
	-@erase "$(INTDIR)\WizardUI.obj"
	-@erase "$(OUTDIR)\WizardMachine.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/WizardMachine.pch" /Yu"stdafx.h"\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/WizardMachine.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/WizardMachine.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=/nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)/WizardMachine.pdb" /machine:I386\
 /out:"$(OUTDIR)/WizardMachine.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ImageDialog.obj" \
	"$(INTDIR)\NavText.obj" \
	"$(INTDIR)\NewConfigDialog.obj" \
	"$(INTDIR)\NewDialog.obj" \
	"$(INTDIR)\ProgDlgThread.obj" \
	"$(INTDIR)\ProgressDialog.obj" \
	"$(INTDIR)\PropSheet.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\WizardMachine.obj" \
	"$(INTDIR)\WizardMachine.res" \
	"$(INTDIR)\WizardMachineDlg.obj" \
	"$(INTDIR)\WizardUI.obj"

"$(OUTDIR)\WizardMachine.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"

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

ALL : "$(OUTDIR)\WizardMachine.exe" "$(OUTDIR)\WizardMachine.bsc"

CLEAN : 
	-@erase "$(INTDIR)\ImageDialog.obj"
	-@erase "$(INTDIR)\ImageDialog.sbr"
	-@erase "$(INTDIR)\NavText.obj"
	-@erase "$(INTDIR)\NavText.sbr"
	-@erase "$(INTDIR)\NewConfigDialog.obj"
	-@erase "$(INTDIR)\NewConfigDialog.sbr"
	-@erase "$(INTDIR)\NewDialog.obj"
	-@erase "$(INTDIR)\NewDialog.sbr"
	-@erase "$(INTDIR)\ProgDlgThread.obj"
	-@erase "$(INTDIR)\ProgDlgThread.sbr"
	-@erase "$(INTDIR)\ProgressDialog.obj"
	-@erase "$(INTDIR)\ProgressDialog.sbr"
	-@erase "$(INTDIR)\PropSheet.obj"
	-@erase "$(INTDIR)\PropSheet.sbr"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\StdAfx.sbr"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(INTDIR)\WizardMachine.obj"
	-@erase "$(INTDIR)\WizardMachine.pch"
	-@erase "$(INTDIR)\WizardMachine.res"
	-@erase "$(INTDIR)\WizardMachine.sbr"
	-@erase "$(INTDIR)\WizardMachineDlg.obj"
	-@erase "$(INTDIR)\WizardMachineDlg.sbr"
	-@erase "$(INTDIR)\WizardUI.obj"
	-@erase "$(INTDIR)\WizardUI.sbr"
	-@erase "$(OUTDIR)\WizardMachine.bsc"
	-@erase "$(OUTDIR)\WizardMachine.exe"
	-@erase "$(OUTDIR)\WizardMachine.ilk"
	-@erase "$(OUTDIR)\WizardMachine.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR /Yu"stdafx.h" /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/WizardMachine.pch"\
 /Yu"stdafx.h" /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/WizardMachine.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/WizardMachine.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\ImageDialog.sbr" \
	"$(INTDIR)\NavText.sbr" \
	"$(INTDIR)\NewConfigDialog.sbr" \
	"$(INTDIR)\NewDialog.sbr" \
	"$(INTDIR)\ProgDlgThread.sbr" \
	"$(INTDIR)\ProgressDialog.sbr" \
	"$(INTDIR)\PropSheet.sbr" \
	"$(INTDIR)\StdAfx.sbr" \
	"$(INTDIR)\WizardMachine.sbr" \
	"$(INTDIR)\WizardMachineDlg.sbr" \
	"$(INTDIR)\WizardUI.sbr"

"$(OUTDIR)\WizardMachine.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=/nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)/WizardMachine.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/WizardMachine.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ImageDialog.obj" \
	"$(INTDIR)\NavText.obj" \
	"$(INTDIR)\NewConfigDialog.obj" \
	"$(INTDIR)\NewDialog.obj" \
	"$(INTDIR)\ProgDlgThread.obj" \
	"$(INTDIR)\ProgressDialog.obj" \
	"$(INTDIR)\PropSheet.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\WizardMachine.obj" \
	"$(INTDIR)\WizardMachine.res" \
	"$(INTDIR)\WizardMachineDlg.obj" \
	"$(INTDIR)\WizardUI.obj"

"$(OUTDIR)\WizardMachine.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "WizardMachine - Win32 Release"
# Name "WizardMachine - Win32 Debug"

!IF  "$(CFG)" == "WizardMachine - Win32 Release"

!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\ReadMe.txt

!IF  "$(CFG)" == "WizardMachine - Win32 Release"

!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\WizardMachine.cpp
DEP_CPP_WIZAR=\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\WizardMachine.obj" : $(SOURCE) $(DEP_CPP_WIZAR) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\WizardMachine.obj" : $(SOURCE) $(DEP_CPP_WIZAR) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\WizardMachine.sbr" : $(SOURCE) $(DEP_CPP_WIZAR) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\StdAfx.cpp
DEP_CPP_STDAF=\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D\
 "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)/WizardMachine.pch" /Yc"stdafx.h"\
 /Fo"$(INTDIR)/" /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\WizardMachine.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"

# ADD CPP /Yc"stdafx.h"

BuildCmds= \
	$(CPP) /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /FR"$(INTDIR)/" /Fp"$(INTDIR)/WizardMachine.pch"\
 /Yc"stdafx.h" /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c $(SOURCE) \
	

"$(INTDIR)\StdAfx.obj" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\StdAfx.sbr" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\WizardMachine.pch" : $(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\WizardMachine.rc
DEP_RSC_WIZARD=\
	".\res\WizardMachine.ico"\
	".\res\WizardMachine.rc2"\
	

"$(INTDIR)\WizardMachine.res" : $(SOURCE) $(DEP_RSC_WIZARD) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\NavText.cpp
DEP_CPP_NAVTE=\
	".\NavText.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\NavText.obj" : $(SOURCE) $(DEP_CPP_NAVTE) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\NavText.obj" : $(SOURCE) $(DEP_CPP_NAVTE) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\NavText.sbr" : $(SOURCE) $(DEP_CPP_NAVTE) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ImageDialog.cpp
DEP_CPP_IMAGE=\
	".\ImageDialog.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\ImageDialog.obj" : $(SOURCE) $(DEP_CPP_IMAGE) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\ImageDialog.obj" : $(SOURCE) $(DEP_CPP_IMAGE) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\ImageDialog.sbr" : $(SOURCE) $(DEP_CPP_IMAGE) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\PropSheet.cpp
DEP_CPP_PROPS=\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\PropSheet.obj" : $(SOURCE) $(DEP_CPP_PROPS) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\PropSheet.obj" : $(SOURCE) $(DEP_CPP_PROPS) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\PropSheet.sbr" : $(SOURCE) $(DEP_CPP_PROPS) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\WizardMachineDlg.cpp
DEP_CPP_WIZARDM=\
	".\ImageDialog.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\WizardMachineDlg.obj" : $(SOURCE) $(DEP_CPP_WIZARDM) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\WizardMachineDlg.obj" : $(SOURCE) $(DEP_CPP_WIZARDM) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\WizardMachineDlg.sbr" : $(SOURCE) $(DEP_CPP_WIZARDM) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ProgressDialog.cpp
DEP_CPP_PROGR=\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\ProgressDialog.obj" : $(SOURCE) $(DEP_CPP_PROGR) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\ProgressDialog.obj" : $(SOURCE) $(DEP_CPP_PROGR) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\ProgressDialog.sbr" : $(SOURCE) $(DEP_CPP_PROGR) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ProgDlgThread.cpp
DEP_CPP_PROGD=\
	".\ProgDlgThread.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\ProgDlgThread.obj" : $(SOURCE) $(DEP_CPP_PROGD) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\ProgDlgThread.obj" : $(SOURCE) $(DEP_CPP_PROGD) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\ProgDlgThread.sbr" : $(SOURCE) $(DEP_CPP_PROGD) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\NewConfigDialog.cpp
DEP_CPP_NEWCO=\
	".\NewConfigDialog.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\NewConfigDialog.obj" : $(SOURCE) $(DEP_CPP_NEWCO) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\NewConfigDialog.obj" : $(SOURCE) $(DEP_CPP_NEWCO) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\NewConfigDialog.sbr" : $(SOURCE) $(DEP_CPP_NEWCO) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\NewDialog.cpp
DEP_CPP_NEWDI=\
	".\NewDialog.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\NewDialog.obj" : $(SOURCE) $(DEP_CPP_NEWDI) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\NewDialog.obj" : $(SOURCE) $(DEP_CPP_NEWDI) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\NewDialog.sbr" : $(SOURCE) $(DEP_CPP_NEWDI) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\WizardUI.cpp
DEP_CPP_WIZARDU=\
	".\ImageDialog.h"\
	".\NavText.h"\
	".\NewConfigDialog.h"\
	".\NewDialog.h"\
	".\ProgDlgThread.h"\
	".\ProgressDialog.h"\
	".\PropSheet.h"\
	".\StdAfx.h"\
	".\WizardMachine.h"\
	".\WizardMachineDlg.h"\
	".\WizardUI.h"\
	

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\WizardUI.obj" : $(SOURCE) $(DEP_CPP_WIZARDU) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\WizardUI.obj" : $(SOURCE) $(DEP_CPP_WIZARDU) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"

"$(INTDIR)\WizardUI.sbr" : $(SOURCE) $(DEP_CPP_WIZARDU) "$(INTDIR)"\
 "$(INTDIR)\WizardMachine.pch"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
