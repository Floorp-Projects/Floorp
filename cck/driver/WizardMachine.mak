# Microsoft Developer Studio Generated NMAKE File, Based on WizardMachine.dsp
!IF "$(CFG)" == ""
CFG=WizardMachine - Win32 Release
!MESSAGE No configuration specified. Defaulting to WizardMachine - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "WizardMachine - Win32 Release" && "$(CFG)" != "WizardMachine - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "WizardMachine.mak" CFG="WizardMachine - Win32 Release"
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

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "WizardMachine - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

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

CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)\WizardMachine.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\WizardMachine.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WizardMachine.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\WizardMachine.pdb" /machine:I386 /out:"$(OUTDIR)\WizardMachine.exe" 
LINK32_OBJS= \
	"$(INTDIR)\WizardMachine.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\NavText.obj" \
	"$(INTDIR)\ImageDialog.obj" \
	"$(INTDIR)\PropSheet.obj" \
	"$(INTDIR)\WizardMachineDlg.obj" \
	"$(INTDIR)\ProgressDialog.obj" \
	"$(INTDIR)\ProgDlgThread.obj" \
	"$(INTDIR)\NewConfigDialog.obj" \
	"$(INTDIR)\NewDialog.obj" \
	"$(INTDIR)\WizardUI.obj" \
	"$(INTDIR)\WizardMachine.res"

"$(OUTDIR)\WizardMachine.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

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
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
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

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\WizardMachine.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\WizardMachine.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WizardMachine.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\WizardMachine.sbr" \
	"$(INTDIR)\StdAfx.sbr" \
	"$(INTDIR)\NavText.sbr" \
	"$(INTDIR)\ImageDialog.sbr" \
	"$(INTDIR)\PropSheet.sbr" \
	"$(INTDIR)\WizardMachineDlg.sbr" \
	"$(INTDIR)\ProgressDialog.sbr" \
	"$(INTDIR)\ProgDlgThread.sbr" \
	"$(INTDIR)\NewConfigDialog.sbr" \
	"$(INTDIR)\NewDialog.sbr" \
	"$(INTDIR)\WizardUI.sbr"

"$(OUTDIR)\WizardMachine.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\WizardMachine.pdb" /debug /machine:I386 /out:"$(OUTDIR)\WizardMachine.exe" 
LINK32_OBJS= \
	"$(INTDIR)\WizardMachine.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\NavText.obj" \
	"$(INTDIR)\ImageDialog.obj" \
	"$(INTDIR)\PropSheet.obj" \
	"$(INTDIR)\WizardMachineDlg.obj" \
	"$(INTDIR)\ProgressDialog.obj" \
	"$(INTDIR)\ProgDlgThread.obj" \
	"$(INTDIR)\NewConfigDialog.obj" \
	"$(INTDIR)\NewDialog.obj" \
	"$(INTDIR)\WizardUI.obj" \
	"$(INTDIR)\WizardMachine.res"

"$(OUTDIR)\WizardMachine.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("WizardMachine.dep")
!INCLUDE "WizardMachine.dep"
!ELSE 
!MESSAGE Warning: cannot find "WizardMachine.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "WizardMachine - Win32 Release" || "$(CFG)" == "WizardMachine - Win32 Debug"
SOURCE=.\ImageDialog.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\ImageDialog.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\ImageDialog.obj"	"$(INTDIR)\ImageDialog.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\NavText.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\NavText.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\NavText.obj"	"$(INTDIR)\NavText.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\NewConfigDialog.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\NewConfigDialog.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\NewConfigDialog.obj"	"$(INTDIR)\NewConfigDialog.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\NewDialog.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\NewDialog.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\NewDialog.obj"	"$(INTDIR)\NewDialog.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\ProgDlgThread.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\ProgDlgThread.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\ProgDlgThread.obj"	"$(INTDIR)\ProgDlgThread.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\ProgressDialog.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\ProgressDialog.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\ProgressDialog.obj"	"$(INTDIR)\ProgressDialog.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\PropSheet.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\PropSheet.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\PropSheet.obj"	"$(INTDIR)\PropSheet.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\StdAfx.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"

CPP_SWITCHES=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Fp"$(INTDIR)\WizardMachine.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\WizardMachine.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"

CPP_SWITCHES=/nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\WizardMachine.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\StdAfx.sbr"	"$(INTDIR)\WizardMachine.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\WizardMachine.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\WizardMachine.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\WizardMachine.obj"	"$(INTDIR)\WizardMachine.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\WizardMachine.rc

"$(INTDIR)\WizardMachine.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\WizardMachineDlg.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\WizardMachineDlg.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\WizardMachineDlg.obj"	"$(INTDIR)\WizardMachineDlg.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 

SOURCE=.\WizardUI.cpp

!IF  "$(CFG)" == "WizardMachine - Win32 Release"


"$(INTDIR)\WizardUI.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ELSEIF  "$(CFG)" == "WizardMachine - Win32 Debug"


"$(INTDIR)\WizardUI.obj"	"$(INTDIR)\WizardUI.sbr" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\WizardMachine.pch"


!ENDIF 


!ENDIF 

