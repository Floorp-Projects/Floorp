# Microsoft Developer Studio Generated NMAKE File, Based on LiveConnectShell.dsp
!IF "$(CFG)" == ""
CFG=LiveConnectShell - Win32 Debug
!MESSAGE No configuration specified. Defaulting to LiveConnectShell - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "LiveConnectShell - Win32 Release" && "$(CFG)" !=\
 "LiveConnectShell - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LiveConnectShell.mak" CFG="LiveConnectShell - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LiveConnectShell - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "LiveConnectShell - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "LiveConnectShell - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\lcshell.exe"

!ELSE 

ALL : "LiveConnect - Win32 Release" "$(OUTDIR)\lcshell.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"LiveConnect - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\js.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\lcshell.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "$(JDK)\include" /I "$(JDK)\include\win32"\
 /I "." /D "_WIN32" /D "WIN32" /D "NDEBUG" /D "XP_PC" /D "_WINDOWS" /D "JSFILE"\
 /D "LIVECONNECT" /Fp"$(INTDIR)\LiveConnectShell.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LiveConnectShell.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\lcshell.pdb" /machine:I386 /out:"$(OUTDIR)\lcshell.exe" 
LINK32_OBJS= \
	"$(INTDIR)\js.obj" \
	"$(OUTDIR)\LiveConnect.lib" \
	"..\Debug\js32.lib"

"$(OUTDIR)\lcshell.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE=$(InputPath)
PostBuild_Desc=Copy DLL(s) to build directory
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "LiveConnect - Win32 Release" "$(OUTDIR)\lcshell.exe"
   COPY ..\Release\js32.dll .\Release
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "LiveConnectShell - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\lcshell.exe" "$(OUTDIR)\LiveConnectShell.bsc"

!ELSE 

ALL : "LiveConnect - Win32 Debug" "$(OUTDIR)\lcshell.exe"\
 "$(OUTDIR)\LiveConnectShell.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"LiveConnect - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\js.obj"
	-@erase "$(INTDIR)\js.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\lcshell.exe"
	-@erase "$(OUTDIR)\lcshell.ilk"
	-@erase "$(OUTDIR)\lcshell.pdb"
	-@erase "$(OUTDIR)\LiveConnectShell.bsc"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "$(JDK)\include" /I\
 "$(JDK)\include\win32" /I "." /D "LIVECONNECT" /D "_WIN32" /D "WIN32" /D\
 "_DEBUG" /D "DEBUG" /D "XP_PC" /D "_WINDOWS" /D "JSFILE" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\LiveConnectShell.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD\
 /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LiveConnectShell.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\js.sbr"

"$(OUTDIR)\LiveConnectShell.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:console /incremental:yes\
 /pdb:"$(OUTDIR)\lcshell.pdb" /debug /machine:I386 /out:"$(OUTDIR)\lcshell.exe"\
 /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\js.obj" \
	"$(OUTDIR)\LiveConnect_g.lib" \
	"..\Debug\js32.lib"

"$(OUTDIR)\lcshell.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE=$(InputPath)
PostBuild_Desc=Copy DLL(s) to build directory
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "LiveConnect - Win32 Debug" "$(OUTDIR)\lcshell.exe"\
 "$(OUTDIR)\LiveConnectShell.bsc"
   COPY ..\Debug\js32.dll .\Debug
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(CFG)" == "LiveConnectShell - Win32 Release" || "$(CFG)" ==\
 "LiveConnectShell - Win32 Debug"

!IF  "$(CFG)" == "LiveConnectShell - Win32 Release"

"LiveConnect - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\LC.mak CFG="LiveConnect - Win32 Release"\
 
   cd "."

"LiveConnect - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\LC.mak\
 CFG="LiveConnect - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "LiveConnectShell - Win32 Debug"

"LiveConnect - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\LC.mak CFG="LiveConnect - Win32 Debug" 
   cd "."

"LiveConnect - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\LC.mak\
 CFG="LiveConnect - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

SOURCE=..\js.c
DEP_CPP_JS_C0=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsdbgapi.h"\
	"..\jsemit.h"\
	"..\jsfun.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsopcode.def"\
	"..\jsopcode.h"\
	"..\jsparse.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscan.h"\
	"..\jsscope.h"\
	"..\jsscript.h"\
	"..\jsstddef.h"\
	"..\jsstr.h"\
	"..\os\aix.h"\
	"..\os\bsdi.h"\
	"..\os\hpux.h"\
	"..\os\irix.h"\
	"..\os\linux.h"\
	"..\os\osf1.h"\
	"..\os\scoos.h"\
	"..\os\solaris.h"\
	"..\os\sunos.h"\
	"..\os\unixware.h"\
	"..\os\win16.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JS_C0=\
	"..\jsdb.h"\
	"..\jsdebug.h"\
	"..\jsdjava.h"\
	"..\jsperl.h"\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	

!IF  "$(CFG)" == "LiveConnectShell - Win32 Release"


"$(INTDIR)\js.obj" : $(SOURCE) $(DEP_CPP_JS_C0) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "LiveConnectShell - Win32 Debug"


"$(INTDIR)\js.obj"	"$(INTDIR)\js.sbr" : $(SOURCE) $(DEP_CPP_JS_C0) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

