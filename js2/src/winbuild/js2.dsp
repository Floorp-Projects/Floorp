# Microsoft Developer Studio Project File - Name="js2" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=js2 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "js2.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "js2.mak" CFG="js2 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "js2 - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "js2 - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "js2 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "js2 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /ZI /Od /I "../../../../../../gc/boehm/" /D "_LIB" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "DEBUG" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "js2 - Win32 Release"
# Name "js2 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\debugger.cpp
# End Source File
# Begin Source File

SOURCE=..\exception.cpp
# End Source File
# Begin Source File

SOURCE=..\formatter.cpp
# End Source File
# Begin Source File

SOURCE=..\hash.cpp
# End Source File
# Begin Source File

SOURCE=..\icode_emitter.cpp
# End Source File
# Begin Source File

SOURCE=..\icodegenerator.cpp
# End Source File
# Begin Source File

SOURCE=..\interpreter.cpp
# End Source File
# Begin Source File

SOURCE=..\jsmath.cpp
# End Source File
# Begin Source File

SOURCE=..\jstypes.cpp

!IF  "$(CFG)" == "js2 - Win32 Release"

!ELSEIF  "$(CFG)" == "js2 - Win32 Debug"

# ADD CPP /I "../../../gc/boehm/"
# SUBTRACT CPP /I "../../../../../../gc/boehm/"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\lexer.cpp
# End Source File
# Begin Source File

SOURCE=..\mem.cpp
# End Source File
# Begin Source File

SOURCE=..\numerics.cpp
# End Source File
# Begin Source File

SOURCE=..\parser.cpp
# End Source File
# Begin Source File

SOURCE=..\reader.cpp
# End Source File
# Begin Source File

SOURCE=..\strings.cpp
# End Source File
# Begin Source File

SOURCE=..\token.cpp
# End Source File
# Begin Source File

SOURCE=..\utilities.cpp
# End Source File
# Begin Source File

SOURCE=..\vmtypes.cpp
# End Source File
# Begin Source File

SOURCE=..\world.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\algo.h
# End Source File
# Begin Source File

SOURCE=..\cpucfg.h
# End Source File
# Begin Source File

SOURCE=..\debugger.h
# End Source File
# Begin Source File

SOURCE=..\ds.h
# End Source File
# Begin Source File

SOURCE=..\exception.h
# End Source File
# Begin Source File

SOURCE=..\formatter.h
# End Source File
# Begin Source File

SOURCE=..\gc_allocator.h
# End Source File
# Begin Source File

SOURCE=..\gc_container.h
# End Source File
# Begin Source File

SOURCE=..\hash.h
# End Source File
# Begin Source File

SOURCE=..\icode.h
# End Source File
# Begin Source File

SOURCE=..\icodegenerator.h
# End Source File
# Begin Source File

SOURCE=..\icodemap.h
# End Source File
# Begin Source File

SOURCE=..\interpreter.h
# End Source File
# Begin Source File

SOURCE=..\jsclasses.h
# End Source File
# Begin Source File

SOURCE=..\jsmath.h
# End Source File
# Begin Source File

SOURCE=..\jstypes.h
# End Source File
# Begin Source File

SOURCE=..\lexer.h
# End Source File
# Begin Source File

SOURCE=..\mem.h
# End Source File
# Begin Source File

SOURCE=..\nodefactory.h
# End Source File
# Begin Source File

SOURCE=..\numerics.h
# End Source File
# Begin Source File

SOURCE=..\parser.h
# End Source File
# Begin Source File

SOURCE=..\reader.h
# End Source File
# Begin Source File

SOURCE=..\stlcfg.h
# End Source File
# Begin Source File

SOURCE=..\strings.h
# End Source File
# Begin Source File

SOURCE=..\systemtypes.h
# End Source File
# Begin Source File

SOURCE=..\token.h
# End Source File
# Begin Source File

SOURCE=..\utilities.h
# End Source File
# Begin Source File

SOURCE=..\vmtypes.h
# End Source File
# Begin Source File

SOURCE=..\world.h
# End Source File
# End Group
# End Target
# End Project
