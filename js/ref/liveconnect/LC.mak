# Microsoft Developer Studio Generated NMAKE File, Based on LiveConnect.dsp
!IF "$(CFG)" == ""
CFG=LiveConnect - Win32 Debug
!MESSAGE No configuration specified. Defaulting to LiveConnect - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "LiveConnect - Win32 Release" && "$(CFG)" !=\
 "LiveConnect - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "LiveConnect.mak" CFG="LiveConnect - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "LiveConnect - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "LiveConnect - Win32 Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\LiveConnect.dll"

!ELSE 

ALL : "$(OUTDIR)\LiveConnect.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\jsj.obj"
	-@erase "$(INTDIR)\jsj_array.obj"
	-@erase "$(INTDIR)\jsj_class.obj"
	-@erase "$(INTDIR)\jsj_convert.obj"
	-@erase "$(INTDIR)\jsj_field.obj"
	-@erase "$(INTDIR)\jsj_hash.obj"
	-@erase "$(INTDIR)\jsj_JavaArray.obj"
	-@erase "$(INTDIR)\jsj_JavaClass.obj"
	-@erase "$(INTDIR)\jsj_JavaMember.obj"
	-@erase "$(INTDIR)\jsj_JavaObject.obj"
	-@erase "$(INTDIR)\jsj_JavaPackage.obj"
	-@erase "$(INTDIR)\jsj_JSObject.obj"
	-@erase "$(INTDIR)\jsj_method.obj"
	-@erase "$(INTDIR)\jsj_utils.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\LiveConnect.dll"
	-@erase "$(OUTDIR)\LiveConnect.exp"
	-@erase "$(OUTDIR)\LiveConnect.lib"
	-@erase "$(OUTDIR)\LiveConnect.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /Zi /O2 /I ".." /I "$(JDK)\include" /I\
 "$(JDK)\include\win32" /D "NDEBUG" /D "XP_PC" /D "JSFILE" /D "WIN32" /D\
 "_WINDOWS" /Fp"$(INTDIR)\LiveConnect.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"\
 /FD /c 
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

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LiveConnect.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\Release\js32.lib $(JDK)\lib\javai.lib /nologo\
 /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\LiveConnect.pdb" /debug\
 /machine:I386 /out:"$(OUTDIR)\LiveConnect.dll"\
 /implib:"$(OUTDIR)\LiveConnect.lib" 
LINK32_OBJS= \
	"$(INTDIR)\jsj.obj" \
	"$(INTDIR)\jsj_array.obj" \
	"$(INTDIR)\jsj_class.obj" \
	"$(INTDIR)\jsj_convert.obj" \
	"$(INTDIR)\jsj_field.obj" \
	"$(INTDIR)\jsj_hash.obj" \
	"$(INTDIR)\jsj_JavaArray.obj" \
	"$(INTDIR)\jsj_JavaClass.obj" \
	"$(INTDIR)\jsj_JavaMember.obj" \
	"$(INTDIR)\jsj_JavaObject.obj" \
	"$(INTDIR)\jsj_JavaPackage.obj" \
	"$(INTDIR)\jsj_JSObject.obj" \
	"$(INTDIR)\jsj_method.obj" \
	"$(INTDIR)\jsj_utils.obj"

"$(OUTDIR)\LiveConnect.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\LiveConnect_g.dll" "$(OUTDIR)\LiveConnect.bsc"

!ELSE 

ALL : "$(OUTDIR)\LiveConnect_g.dll" "$(OUTDIR)\LiveConnect.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\jsj.obj"
	-@erase "$(INTDIR)\jsj.sbr"
	-@erase "$(INTDIR)\jsj_array.obj"
	-@erase "$(INTDIR)\jsj_array.sbr"
	-@erase "$(INTDIR)\jsj_class.obj"
	-@erase "$(INTDIR)\jsj_class.sbr"
	-@erase "$(INTDIR)\jsj_convert.obj"
	-@erase "$(INTDIR)\jsj_convert.sbr"
	-@erase "$(INTDIR)\jsj_field.obj"
	-@erase "$(INTDIR)\jsj_field.sbr"
	-@erase "$(INTDIR)\jsj_hash.obj"
	-@erase "$(INTDIR)\jsj_hash.sbr"
	-@erase "$(INTDIR)\jsj_JavaArray.obj"
	-@erase "$(INTDIR)\jsj_JavaArray.sbr"
	-@erase "$(INTDIR)\jsj_JavaClass.obj"
	-@erase "$(INTDIR)\jsj_JavaClass.sbr"
	-@erase "$(INTDIR)\jsj_JavaMember.obj"
	-@erase "$(INTDIR)\jsj_JavaMember.sbr"
	-@erase "$(INTDIR)\jsj_JavaObject.obj"
	-@erase "$(INTDIR)\jsj_JavaObject.sbr"
	-@erase "$(INTDIR)\jsj_JavaPackage.obj"
	-@erase "$(INTDIR)\jsj_JavaPackage.sbr"
	-@erase "$(INTDIR)\jsj_JSObject.obj"
	-@erase "$(INTDIR)\jsj_JSObject.sbr"
	-@erase "$(INTDIR)\jsj_method.obj"
	-@erase "$(INTDIR)\jsj_method.sbr"
	-@erase "$(INTDIR)\jsj_utils.obj"
	-@erase "$(INTDIR)\jsj_utils.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\LiveConnect.bsc"
	-@erase "$(OUTDIR)\LiveConnect_g.dll"
	-@erase "$(OUTDIR)\LiveConnect_g.exp"
	-@erase "$(OUTDIR)\LiveConnect_g.ilk"
	-@erase "$(OUTDIR)\LiveConnect_g.lib"
	-@erase "$(OUTDIR)\LiveConnect_g.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I ".." /I "$(JDK)\include" /I\
 "$(JDK)\include\win32" /D "_DEBUG" /D "_CONSOLE" /D "DEBUG" /D "XP_PC" /D\
 "JSFILE" /D "WIN32" /D "_WINDOWS" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\LiveConnect.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
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

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\LiveConnect.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\jsj.sbr" \
	"$(INTDIR)\jsj_array.sbr" \
	"$(INTDIR)\jsj_class.sbr" \
	"$(INTDIR)\jsj_convert.sbr" \
	"$(INTDIR)\jsj_field.sbr" \
	"$(INTDIR)\jsj_hash.sbr" \
	"$(INTDIR)\jsj_JavaArray.sbr" \
	"$(INTDIR)\jsj_JavaClass.sbr" \
	"$(INTDIR)\jsj_JavaMember.sbr" \
	"$(INTDIR)\jsj_JavaObject.sbr" \
	"$(INTDIR)\jsj_JavaPackage.sbr" \
	"$(INTDIR)\jsj_JSObject.sbr" \
	"$(INTDIR)\jsj_method.sbr" \
	"$(INTDIR)\jsj_utils.sbr"

"$(OUTDIR)\LiveConnect.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\Debug\js32.lib $(JDK)\lib\javai_g.lib /nologo\
 /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)\LiveConnect_g.pdb"\
 /debug /machine:I386 /out:"$(OUTDIR)\LiveConnect_g.dll"\
 /implib:"$(OUTDIR)\LiveConnect_g.lib" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\jsj.obj" \
	"$(INTDIR)\jsj_array.obj" \
	"$(INTDIR)\jsj_class.obj" \
	"$(INTDIR)\jsj_convert.obj" \
	"$(INTDIR)\jsj_field.obj" \
	"$(INTDIR)\jsj_hash.obj" \
	"$(INTDIR)\jsj_JavaArray.obj" \
	"$(INTDIR)\jsj_JavaClass.obj" \
	"$(INTDIR)\jsj_JavaMember.obj" \
	"$(INTDIR)\jsj_JavaObject.obj" \
	"$(INTDIR)\jsj_JavaPackage.obj" \
	"$(INTDIR)\jsj_JSObject.obj" \
	"$(INTDIR)\jsj_method.obj" \
	"$(INTDIR)\jsj_utils.obj"

"$(OUTDIR)\LiveConnect_g.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "LiveConnect - Win32 Release" || "$(CFG)" ==\
 "LiveConnect - Win32 Debug"
SOURCE=..\liveconnect\jsj.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_C=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
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
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	".\netscape_javascript_JSObject.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_C=\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj.obj" : $(SOURCE) $(DEP_CPP_JSJ_C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_C=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj.obj"	"$(INTDIR)\jsj.sbr" : $(SOURCE) $(DEP_CPP_JSJ_C)\
 "$(INTDIR)"


!ENDIF 

SOURCE=..\liveconnect\jsj_array.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_A=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
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
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_A=\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_array.obj" : $(SOURCE) $(DEP_CPP_JSJ_A) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_A=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_array.obj"	"$(INTDIR)\jsj_array.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_A) "$(INTDIR)"


!ENDIF 

SOURCE=..\liveconnect\jsj_class.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_CL=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
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
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_CL=\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_class.obj" : $(SOURCE) $(DEP_CPP_JSJ_CL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_CL=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_class.obj"	"$(INTDIR)\jsj_class.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_CL) "$(INTDIR)"


!ENDIF 

SOURCE=..\liveconnect\jsj_convert.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_CO=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_CO=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_convert.obj" : $(SOURCE) $(DEP_CPP_JSJ_CO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_CO=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_convert.obj"	"$(INTDIR)\jsj_convert.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_CO) "$(INTDIR)"


!ENDIF 

SOURCE=..\liveconnect\jsj_field.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_F=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
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
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_F=\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_field.obj" : $(SOURCE) $(DEP_CPP_JSJ_F) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_F=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_field.obj"	"$(INTDIR)\jsj_field.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_F) "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_hash.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_H=\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prtypes.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\jsj_hash.obj" : $(SOURCE) $(DEP_CPP_JSJ_H) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_H=\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	

"$(INTDIR)\jsj_hash.obj"	"$(INTDIR)\jsj_hash.sbr" : $(SOURCE) $(DEP_CPP_JSJ_H)\
 "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_JavaArray.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_J=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_J=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_JavaArray.obj" : $(SOURCE) $(DEP_CPP_JSJ_J) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_J=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_JavaArray.obj"	"$(INTDIR)\jsj_JavaArray.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_J) "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_JavaClass.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_JA=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_JA=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_JavaClass.obj" : $(SOURCE) $(DEP_CPP_JSJ_JA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_JA=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_JavaClass.obj"	"$(INTDIR)\jsj_JavaClass.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_JA) "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_JavaMember.c
DEP_CPP_JSJ_JAV=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
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
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_JAV=\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

!IF  "$(CFG)" == "LiveConnect - Win32 Release"


"$(INTDIR)\jsj_JavaMember.obj" : $(SOURCE) $(DEP_CPP_JSJ_JAV) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"


"$(INTDIR)\jsj_JavaMember.obj"	"$(INTDIR)\jsj_JavaMember.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_JAV) "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_JavaObject.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_JAVA=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_JAVA=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_JavaObject.obj" : $(SOURCE) $(DEP_CPP_JSJ_JAVA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_JAVA=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_JavaObject.obj"	"$(INTDIR)\jsj_JavaObject.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_JAVA) "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_JavaPackage.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_JAVAP=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_JAVAP=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_JavaPackage.obj" : $(SOURCE) $(DEP_CPP_JSJ_JAVAP) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_JAVAP=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_JavaPackage.obj"	"$(INTDIR)\jsj_JavaPackage.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_JAVAP) "$(INTDIR)"


!ENDIF 

SOURCE=.\jsj_JSObject.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_JS=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	".\netscape_javascript_JSObject.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_JS=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	
CPP_SWITCHES=/nologo /MD /W3 /GX /Zi /O2 /I ".." /I "$(JDK)\include" /I\
 "$(JDK)\include\win32" /D "NDEBUG" /D "XP_PC" /D "JSFILE" /D "WIN32" /D\
 "_WINDOWS" /Fp"$(INTDIR)\LiveConnect.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"\
 /FD /c 

"$(INTDIR)\jsj_JSObject.obj" : $(SOURCE) $(DEP_CPP_JSJ_JS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_JS=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	".\netscape_javascript_JSObject.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	
CPP_SWITCHES=/nologo /MDd /W3 /Gm /GX /Zi /Od /I ".." /I "$(JDK)\include" /I\
 "$(JDK)\include\win32" /I ".\_jni" /D "_DEBUG" /D "_CONSOLE" /D "DEBUG" /D\
 "XP_PC" /D "JSFILE" /D "WIN32" /D "_WINDOWS" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\LiveConnect.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\jsj_JSObject.obj"	"$(INTDIR)\jsj_JSObject.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_JS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\liveconnect\jsj_method.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_M=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jslock.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsscope.h"\
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
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_M=\
	"..\prcvar.h"\
	"..\prlock.h"\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_method.obj" : $(SOURCE) $(DEP_CPP_JSJ_M) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_M=\
	"..\jsapi.h"\
	"..\jsatom.h"\
	"..\jscntxt.h"\
	"..\jsgc.h"\
	"..\jsinterp.h"\
	"..\jsmsg.def"\
	"..\jsobj.h"\
	"..\jsprvtd.h"\
	"..\jspubtd.h"\
	"..\jsregexp.h"\
	"..\jsstr.h"\
	"..\os\win32.h"\
	"..\prarena.h"\
	"..\prassert.h"\
	"..\prclist.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prlong.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_method.obj"	"$(INTDIR)\jsj_method.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_M) "$(INTDIR)"


!ENDIF 

SOURCE=..\liveconnect\jsj_utils.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

DEP_CPP_JSJ_U=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
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
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prmacos.h"\
	"..\prosdep.h"\
	"..\prpcos.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	"..\prunixos.h"\
	"..\sunos4.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_JSJ_U=\
	".\jni.h"\
	".\prlog.h"\
	".\prprf.h"\
	

"$(INTDIR)\jsj_utils.obj" : $(SOURCE) $(DEP_CPP_JSJ_U) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

DEP_CPP_JSJ_U=\
	"..\jsapi.h"\
	"..\jspubtd.h"\
	"..\prassert.h"\
	"..\prcpucfg.h"\
	"..\prhash.h"\
	"..\prprintf.h"\
	"..\prtypes.h"\
	".\jsj_hash.h"\
	".\jsj_private.h"\
	".\jsjava.h"\
	"c:\jdk1.1.6\include\jni.h"\
	"c:\jdk1.1.6\include\win32\jni_md.h"\
	

"$(INTDIR)\jsj_utils.obj"	"$(INTDIR)\jsj_utils.sbr" : $(SOURCE)\
 $(DEP_CPP_JSJ_U) "$(INTDIR)"


!ENDIF 


!ENDIF 

