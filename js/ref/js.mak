# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103
# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=jsshell - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to jsshell - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "jsshell - Win32 Release" && "$(CFG)" !=\
 "jsshell - Win32 Debug" && "$(CFG)" != "js32 - Win32 Release" && "$(CFG)" !=\
 "js32 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "js.mak" CFG="jsshell - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "jsshell - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "jsshell - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "js32 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "js32 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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
# PROP Target_Last_Scanned "js32 - Win32 Debug"

!IF  "$(CFG)" == "jsshell - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "jsshell_"
# PROP BASE Intermediate_Dir "jsshell_"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "js32 - Win32 Release" "$(OUTDIR)\jsshell.exe"

CLEAN : 
	-@erase "$(INTDIR)\js.obj"
	-@erase "$(OUTDIR)\jsshell.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_WIN32" /D "WIN32" /D "XP_PC" /D "_WINDOWS" /D "JSFILE" /YX /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "_CONSOLE" /D "_WIN32" /D\
 "WIN32" /D "XP_PC" /D "_WINDOWS" /D "JSFILE" /Fp"$(INTDIR)/js.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/js.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"Release\jsshell.exe"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)/jsshell.pdb" /machine:I386 /out:"$(OUTDIR)/jsshell.exe" 
LINK32_OBJS= \
	"$(INTDIR)\js.obj" \
	"$(OUTDIR)\js32.lib"

"$(OUTDIR)\jsshell.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "jsshell - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "jsshell0"
# PROP BASE Intermediate_Dir "jsshell0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "js32 - Win32 Debug" "$(OUTDIR)\jsshell.exe"

CLEAN : 
	-@erase "$(INTDIR)\js.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\jsshell.exe"
	-@erase "$(OUTDIR)\jsshell.ilk"
	-@erase "$(OUTDIR)\jsshell.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "_CONSOLE" /D "_WIN32" /D "WIN32" /D "DEBUG" /D "XP_PC" /D "_WINDOWS" /D "JSFILE" /YX /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "_CONSOLE" /D "_WIN32"\
 /D "WIN32" /D "DEBUG" /D "XP_PC" /D "_WINDOWS" /D "JSFILE"\
 /Fp"$(INTDIR)/js.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.

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

RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/js.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug\jsshell.exe"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:console /incremental:yes\
 /pdb:"$(OUTDIR)/jsshell.pdb" /debug /machine:I386 /out:"$(OUTDIR)/jsshell.exe" 
LINK32_OBJS= \
	"$(INTDIR)\js.obj" \
	"$(OUTDIR)\js32.lib"

"$(OUTDIR)\jsshell.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "js32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "js32\Release"
# PROP BASE Intermediate_Dir "js32\Release"
# PROP BASE Target_Dir "js32"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir "js32"
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\js32.dll"

CLEAN : 
	-@erase "$(INTDIR)\jsapi.obj"
	-@erase "$(INTDIR)\jsarray.obj"
	-@erase "$(INTDIR)\jsatom.obj"
	-@erase "$(INTDIR)\jsbool.obj"
	-@erase "$(INTDIR)\jscntxt.obj"
	-@erase "$(INTDIR)\jsdate.obj"
	-@erase "$(INTDIR)\jsdbgapi.obj"
	-@erase "$(INTDIR)\jsemit.obj"
	-@erase "$(INTDIR)\jsfun.obj"
	-@erase "$(INTDIR)\jsgc.obj"
	-@erase "$(INTDIR)\jsinterp.obj"
	-@erase "$(INTDIR)\jslock.obj"
	-@erase "$(INTDIR)\jsmath.obj"
	-@erase "$(INTDIR)\jsnum.obj"
	-@erase "$(INTDIR)\jsobj.obj"
	-@erase "$(INTDIR)\jsopcode.obj"
	-@erase "$(INTDIR)\jsparse.obj"
	-@erase "$(INTDIR)\jsregexp.obj"
	-@erase "$(INTDIR)\jsscan.obj"
	-@erase "$(INTDIR)\jsscope.obj"
	-@erase "$(INTDIR)\jsscript.obj"
	-@erase "$(INTDIR)\jsstr.obj"
	-@erase "$(INTDIR)\jsxdrapi.obj"
	-@erase "$(INTDIR)\prarena.obj"
	-@erase "$(INTDIR)\prassert.obj"
	-@erase "$(INTDIR)\prdtoa.obj"
	-@erase "$(INTDIR)\prhash.obj"
	-@erase "$(INTDIR)\prlog2.obj"
	-@erase "$(INTDIR)\prlong.obj"
	-@erase "$(INTDIR)\prprintf.obj"
	-@erase "$(INTDIR)\prtime.obj"
	-@erase "$(OUTDIR)\js32.dll"
	-@erase "$(OUTDIR)\js32.exp"
	-@erase "$(OUTDIR)\js32.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "_WINDOWS" /D "_WIN32" /D "WIN32" /D "XP_PC" /D "JSFILE" /D "EXPORT_JS_API" /YX /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "_WINDOWS" /D "_WIN32" /D\
 "WIN32" /D "XP_PC" /D "JSFILE" /D "EXPORT_JS_API" /Fp"$(INTDIR)/js32.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.

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

MTL=mktyplib.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/js32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/js32.pdb" /machine:I386 /out:"$(OUTDIR)/js32.dll"\
 /implib:"$(OUTDIR)/js32.lib" 
LINK32_OBJS= \
	"$(INTDIR)\jsapi.obj" \
	"$(INTDIR)\jsarray.obj" \
	"$(INTDIR)\jsatom.obj" \
	"$(INTDIR)\jsbool.obj" \
	"$(INTDIR)\jscntxt.obj" \
	"$(INTDIR)\jsdate.obj" \
	"$(INTDIR)\jsdbgapi.obj" \
	"$(INTDIR)\jsemit.obj" \
	"$(INTDIR)\jsfun.obj" \
	"$(INTDIR)\jsgc.obj" \
	"$(INTDIR)\jsinterp.obj" \
	"$(INTDIR)\jslock.obj" \
	"$(INTDIR)\jsmath.obj" \
	"$(INTDIR)\jsnum.obj" \
	"$(INTDIR)\jsobj.obj" \
	"$(INTDIR)\jsopcode.obj" \
	"$(INTDIR)\jsparse.obj" \
	"$(INTDIR)\jsregexp.obj" \
	"$(INTDIR)\jsscan.obj" \
	"$(INTDIR)\jsscope.obj" \
	"$(INTDIR)\jsscript.obj" \
	"$(INTDIR)\jsstr.obj" \
	"$(INTDIR)\jsxdrapi.obj" \
	"$(INTDIR)\prarena.obj" \
	"$(INTDIR)\prassert.obj" \
	"$(INTDIR)\prdtoa.obj" \
	"$(INTDIR)\prhash.obj" \
	"$(INTDIR)\prlog2.obj" \
	"$(INTDIR)\prlong.obj" \
	"$(INTDIR)\prprintf.obj" \
	"$(INTDIR)\prtime.obj"

"$(OUTDIR)\js32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "js32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "js32\Debug"
# PROP BASE Intermediate_Dir "js32\Debug"
# PROP BASE Target_Dir "js32"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir "js32"
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\js32.dll"

CLEAN : 
	-@erase "$(INTDIR)\jsapi.obj"
	-@erase "$(INTDIR)\jsarray.obj"
	-@erase "$(INTDIR)\jsatom.obj"
	-@erase "$(INTDIR)\jsbool.obj"
	-@erase "$(INTDIR)\jscntxt.obj"
	-@erase "$(INTDIR)\jsdate.obj"
	-@erase "$(INTDIR)\jsdbgapi.obj"
	-@erase "$(INTDIR)\jsemit.obj"
	-@erase "$(INTDIR)\jsfun.obj"
	-@erase "$(INTDIR)\jsgc.obj"
	-@erase "$(INTDIR)\jsinterp.obj"
	-@erase "$(INTDIR)\jslock.obj"
	-@erase "$(INTDIR)\jsmath.obj"
	-@erase "$(INTDIR)\jsnum.obj"
	-@erase "$(INTDIR)\jsobj.obj"
	-@erase "$(INTDIR)\jsopcode.obj"
	-@erase "$(INTDIR)\jsparse.obj"
	-@erase "$(INTDIR)\jsregexp.obj"
	-@erase "$(INTDIR)\jsscan.obj"
	-@erase "$(INTDIR)\jsscope.obj"
	-@erase "$(INTDIR)\jsscript.obj"
	-@erase "$(INTDIR)\jsstr.obj"
	-@erase "$(INTDIR)\jsxdrapi.obj"
	-@erase "$(INTDIR)\prarena.obj"
	-@erase "$(INTDIR)\prassert.obj"
	-@erase "$(INTDIR)\prdtoa.obj"
	-@erase "$(INTDIR)\prhash.obj"
	-@erase "$(INTDIR)\prlog2.obj"
	-@erase "$(INTDIR)\prlong.obj"
	-@erase "$(INTDIR)\prprintf.obj"
	-@erase "$(INTDIR)\prtime.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\js32.dll"
	-@erase "$(OUTDIR)\js32.exp"
	-@erase "$(OUTDIR)\js32.ilk"
	-@erase "$(OUTDIR)\js32.lib"
	-@erase "$(OUTDIR)\js32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "DEBUG" /D "_WINDOWS" /D "_WIN32" /D "WIN32" /D "XP_PC" /D "JSFILE" /D "EXPORT_JS_API" /YX /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "DEBUG" /D "_WINDOWS"\
 /D "_WIN32" /D "WIN32" /D "XP_PC" /D "JSFILE" /D "EXPORT_JS_API"\
 /Fp"$(INTDIR)/js32.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.

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

MTL=mktyplib.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/js32.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/js32.pdb" /debug /machine:I386 /out:"$(OUTDIR)/js32.dll"\
 /implib:"$(OUTDIR)/js32.lib" 
LINK32_OBJS= \
	"$(INTDIR)\jsapi.obj" \
	"$(INTDIR)\jsarray.obj" \
	"$(INTDIR)\jsatom.obj" \
	"$(INTDIR)\jsbool.obj" \
	"$(INTDIR)\jscntxt.obj" \
	"$(INTDIR)\jsdate.obj" \
	"$(INTDIR)\jsdbgapi.obj" \
	"$(INTDIR)\jsemit.obj" \
	"$(INTDIR)\jsfun.obj" \
	"$(INTDIR)\jsgc.obj" \
	"$(INTDIR)\jsinterp.obj" \
	"$(INTDIR)\jslock.obj" \
	"$(INTDIR)\jsmath.obj" \
	"$(INTDIR)\jsnum.obj" \
	"$(INTDIR)\jsobj.obj" \
	"$(INTDIR)\jsopcode.obj" \
	"$(INTDIR)\jsparse.obj" \
	"$(INTDIR)\jsregexp.obj" \
	"$(INTDIR)\jsscan.obj" \
	"$(INTDIR)\jsscope.obj" \
	"$(INTDIR)\jsscript.obj" \
	"$(INTDIR)\jsstr.obj" \
	"$(INTDIR)\jsxdrapi.obj" \
	"$(INTDIR)\prarena.obj" \
	"$(INTDIR)\prassert.obj" \
	"$(INTDIR)\prdtoa.obj" \
	"$(INTDIR)\prhash.obj" \
	"$(INTDIR)\prlog2.obj" \
	"$(INTDIR)\prlong.obj" \
	"$(INTDIR)\prprintf.obj" \
	"$(INTDIR)\prtime.obj"

"$(OUTDIR)\js32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

################################################################################
# Begin Target

# Name "jsshell - Win32 Release"
# Name "jsshell - Win32 Debug"

!IF  "$(CFG)" == "jsshell - Win32 Release"

!ELSEIF  "$(CFG)" == "jsshell - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\js.c
DEP_CPP_JS_C0=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsdbgapi.h"\
	".\jsemit.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsparse.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscan.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JS_C0=\
	".\jsdebug.h"\
	".\jsdjava.h"\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\js.obj" : $(SOURCE) $(DEP_CPP_JS_C0) "$(INTDIR)"


# End Source File
################################################################################
# Begin Project Dependency

# Project_Dep_Name "js32"

!IF  "$(CFG)" == "jsshell - Win32 Release"

"js32 - Win32 Release" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\js.mak" CFG="js32 - Win32 Release" 

!ELSEIF  "$(CFG)" == "jsshell - Win32 Debug"

"js32 - Win32 Debug" : 
   $(MAKE) /$(MAKEFLAGS) /F ".\js.mak" CFG="js32 - Win32 Debug" 

!ENDIF 

# End Project Dependency
# End Target
################################################################################
# Begin Target

# Name "js32 - Win32 Release"
# Name "js32 - Win32 Debug"

!IF  "$(CFG)" == "js32 - Win32 Release"

!ELSEIF  "$(CFG)" == "js32 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\prtime.c
DEP_CPP_PRTIM=\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prcpucfg.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtime.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TIMEB.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prtime.obj" : $(SOURCE) $(DEP_CPP_PRTIM) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsarray.c
DEP_CPP_JSARR=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSARR=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsarray.obj" : $(SOURCE) $(DEP_CPP_JSARR) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsatom.c
DEP_CPP_JSATO=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSATO=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsatom.obj" : $(SOURCE) $(DEP_CPP_JSATO) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsbool.c
DEP_CPP_JSBOO=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jsbool.h"\
	".\jscntxt.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSBOO=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsbool.obj" : $(SOURCE) $(DEP_CPP_JSBOO) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jscntxt.c
DEP_CPP_JSCNT=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdbgapi.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscan.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSCNT=\
	".\jsdebug.h"\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jscntxt.obj" : $(SOURCE) $(DEP_CPP_JSCNT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsdate.c
DEP_CPP_JSDAT=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdate.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtime.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSDAT=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsdate.obj" : $(SOURCE) $(DEP_CPP_JSDAT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsdbgapi.c
DEP_CPP_JSDBG=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdbgapi.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSDBG=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsdbgapi.obj" : $(SOURCE) $(DEP_CPP_JSDBG) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsemit.c
DEP_CPP_JSEMI=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsemit.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSEMI=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsemit.obj" : $(SOURCE) $(DEP_CPP_JSEMI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsfun.c
DEP_CPP_JSFUN=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsparse.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscan.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSFUN=\
	".\jsdebug.h"\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsfun.obj" : $(SOURCE) $(DEP_CPP_JSFUN) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsgc.c
DEP_CPP_JSGC_=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSGC_=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsgc.obj" : $(SOURCE) $(DEP_CPP_JSGC_) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsinterp.c
DEP_CPP_JSINT=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jsbool.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdbgapi.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSINT=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsinterp.obj" : $(SOURCE) $(DEP_CPP_JSINT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jslock.c
DEP_CPP_JSLOC=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSLOC=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	".\prthread.h"\
	

"$(INTDIR)\jslock.obj" : $(SOURCE) $(DEP_CPP_JSLOC) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsmath.c
DEP_CPP_JSMAT=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsmath.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtime.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSMAT=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsmath.obj" : $(SOURCE) $(DEP_CPP_JSMAT) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsnum.c
DEP_CPP_JSNUM=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prdtoa.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSNUM=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsnum.obj" : $(SOURCE) $(DEP_CPP_JSNUM) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsobj.c
DEP_CPP_JSOBJ=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jsbool.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdbgapi.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSOBJ=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsobj.obj" : $(SOURCE) $(DEP_CPP_JSOBJ) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsopcode.c
DEP_CPP_JSOPC=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdbgapi.h"\
	".\jsemit.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prdtoa.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSOPC=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsopcode.obj" : $(SOURCE) $(DEP_CPP_JSOPC) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsparse.c
DEP_CPP_JSPAR=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsemit.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsparse.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscan.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSPAR=\
	".\jsdebug.h"\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsparse.obj" : $(SOURCE) $(DEP_CPP_JSPAR) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsregexp.c
DEP_CPP_JSREG=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSREG=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsregexp.obj" : $(SOURCE) $(DEP_CPP_JSREG) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsscan.c
DEP_CPP_JSSCA=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsemit.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscan.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prdtoa.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSSCA=\
	".\jsdebug.h"\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsscan.obj" : $(SOURCE) $(DEP_CPP_JSSCA) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsscope.c
DEP_CPP_JSSCO=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSSCO=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsscope.obj" : $(SOURCE) $(DEP_CPP_JSSCO) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsscript.c
DEP_CPP_JSSCR=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdbgapi.h"\
	".\jsemit.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\jsxdrapi.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSSCR=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsscript.obj" : $(SOURCE) $(DEP_CPP_JSSCR) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsstr.c
DEP_CPP_JSSTR=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jsbool.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscope.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSSTR=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsstr.obj" : $(SOURCE) $(DEP_CPP_JSSTR) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prarena.c
DEP_CPP_PRARE=\
	".\prarena.h"\
	".\prassert.h"\
	".\prcpucfg.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prarena.obj" : $(SOURCE) $(DEP_CPP_PRARE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prassert.c
DEP_CPP_PRASS=\
	".\prassert.h"\
	".\prcpucfg.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prassert.obj" : $(SOURCE) $(DEP_CPP_PRASS) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prdtoa.c
DEP_CPP_PRDTO=\
	".\prcpucfg.h"\
	".\prdtoa.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prdtoa.obj" : $(SOURCE) $(DEP_CPP_PRDTO) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prhash.c
DEP_CPP_PRHAS=\
	".\prassert.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prhash.obj" : $(SOURCE) $(DEP_CPP_PRHAS) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prlog2.c
DEP_CPP_PRLOG=\
	".\prcpucfg.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prlog2.obj" : $(SOURCE) $(DEP_CPP_PRLOG) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prlong.c
DEP_CPP_PRLON=\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prcpucfg.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prlong.obj" : $(SOURCE) $(DEP_CPP_PRLON) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\prprintf.c
DEP_CPP_PRPRI=\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prassert.h"\
	".\prcpucfg.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prprintf.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	

"$(INTDIR)\prprintf.obj" : $(SOURCE) $(DEP_CPP_PRPRI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsapi.c
DEP_CPP_JSAPI=\
	".\jsapi.h"\
	".\jsarray.h"\
	".\jsatom.h"\
	".\jsbool.h"\
	".\jscntxt.h"\
	".\jsconfig.h"\
	".\jsdate.h"\
	".\jsemit.h"\
	".\jsfun.h"\
	".\jsgc.h"\
	".\jsinterp.h"\
	".\jslock.h"\
	".\jsmath.h"\
	".\jsnum.h"\
	".\jsobj.h"\
	".\jsopcode.def"\
	".\jsopcode.h"\
	".\jsparse.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsregexp.h"\
	".\jsscan.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsstr.h"\
	".\os\aix.h"\
	".\os\bsdi.h"\
	".\os\hpux.h"\
	".\os\irix.h"\
	".\os\linux.h"\
	".\os\osf1.h"\
	".\os\scoos.h"\
	".\os\solaris.h"\
	".\os\sunos.h"\
	".\os\unixware.h"\
	".\os\win16.h"\
	".\os\win32.h"\
	".\prarena.h"\
	".\prassert.h"\
	".\prclist.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prlong.h"\
	".\prmacos.h"\
	".\prosdep.h"\
	".\prpcos.h"\
	".\prtypes.h"\
	".\prunixos.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\STAT.H"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSAPI=\
	".\jsdebug.h"\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsapi.obj" : $(SOURCE) $(DEP_CPP_JSAPI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\jsxdrapi.c

!IF  "$(CFG)" == "js32 - Win32 Release"

DEP_CPP_JSXDR=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsxdrapi.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSXDR=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsxdrapi.obj" : $(SOURCE) $(DEP_CPP_JSXDR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "js32 - Win32 Debug"

DEP_CPP_JSXDR=\
	".\jsapi.h"\
	".\jsatom.h"\
	".\jslock.h"\
	".\jsobj.h"\
	".\jsprvtd.h"\
	".\jspubtd.h"\
	".\jsscope.h"\
	".\jsscript.h"\
	".\jsxdrapi.h"\
	".\prcpucfg.h"\
	".\prhash.h"\
	".\prtypes.h"\
	".\sunos4.h"\
	{$(INCLUDE)}"\sys\TYPES.H"\
	
NODEP_CPP_JSXDR=\
	".\pratomic.h"\
	".\prcvar.h"\
	".\prlock.h"\
	

"$(INTDIR)\jsxdrapi.obj" : $(SOURCE) $(DEP_CPP_JSXDR) "$(INTDIR)"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
