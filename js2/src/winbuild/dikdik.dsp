# Microsoft Developer Studio Project File - Name="DikDik" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=DikDik - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "DikDik.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "DikDik.mak" CFG="DikDik - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DikDik - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "DikDik - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "DikDik - Win32 Release"

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
# ADD CPP /nologo /GX /O2 /Ob2 /D "_LIB" /D "NDEBUG" /D "WIN32" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DikDik___Win32_Debug"
# PROP BASE Intermediate_Dir "DikDik___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DikDik___Win32_Debug"
# PROP Intermediate_Dir "DikDik___Win32_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GR /GX /ZI /Od /I "../../../js/src/fdlibm" /D "_LIB" /D "DEBUG" /D "_DEBUG" /D "WIN32" /D "_MBCS" /FR /YX /FD /GZ /c
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

# Name "DikDik - Win32 Release"
# Name "DikDik - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\bytecodegen.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\exception.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\fdlibm_ns.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\formatter.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\hash.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\js2execution.cpp
# End Source File
# Begin Source File

SOURCE=..\js2runtime.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\jsarray.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\jsmath.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\jsstring.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\lexer.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\mem.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\numerics.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\parser.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\reader.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\strings.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\token.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\tracer.cpp
# End Source File
# Begin Source File

SOURCE=..\utilities.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\world.cpp

!IF  "$(CFG)" == "DikDik - Win32 Release"

# ADD CPP /W4 /GR

!ELSEIF  "$(CFG)" == "DikDik - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\algo.h
# End Source File
# Begin Source File

SOURCE=..\bytecodegen.h
# End Source File
# Begin Source File

SOURCE=..\ds.h
# End Source File
# Begin Source File

SOURCE=..\exception.h
# End Source File
# Begin Source File

SOURCE=..\fdlibm_ns.h
# End Source File
# Begin Source File

SOURCE=..\formatter.h
# End Source File
# Begin Source File

SOURCE=..\hash.h
# End Source File
# Begin Source File

SOURCE=..\js2runtime.h
# End Source File
# Begin Source File

SOURCE=..\jsarray.h
# End Source File
# Begin Source File

SOURCE=..\jsmath.h
# End Source File
# Begin Source File

SOURCE=..\jsstring.h
# End Source File
# Begin Source File

SOURCE=..\lexer.h
# End Source File
# Begin Source File

SOURCE=..\mem.h
# End Source File
# Begin Source File

SOURCE=..\numerics.h
# End Source File
# Begin Source File

SOURCE=..\parser.h
# End Source File
# Begin Source File

SOURCE=..\property.h
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

SOURCE=..\world.h
# End Source File
# End Group
# End Target
# End Project
