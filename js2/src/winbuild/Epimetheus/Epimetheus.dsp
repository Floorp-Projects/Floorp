# Microsoft Developer Studio Project File - Name="Epimetheus" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Epimetheus - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Epimetheus.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Epimetheus.mak" CFG="Epimetheus - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Epimetheus - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Epimetheus - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Epimetheus - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\js\src" /I "../../RegExp" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "XP_PC" /D "EPIMETHEUS" /D "IS_LITTLE_ENDIAN" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /libpath:"..\..\..\..\js\src\release"

!ELSEIF  "$(CFG)" == "Epimetheus - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GR /GX /ZI /Od /I "../../RegExp" /D "_DEBUG" /D "DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "XP_PC" /D "EPIMETHEUS" /D "IS_LITTLE_ENDIAN" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"..\..\..\..\js\src\debug"

!ENDIF 

# Begin Target

# Name "Epimetheus - Win32 Release"
# Name "Epimetheus - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\bytecodecontainer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\epimetheus.cpp
# End Source File
# Begin Source File

SOURCE=..\..\exception.cpp
# End Source File
# Begin Source File

SOURCE=..\..\formatter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\hash.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2array.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2boolean.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2date.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2engine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2error.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2eval.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2math.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2metadata.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2number.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2regexp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\js2string.cpp
# End Source File
# Begin Source File

SOURCE=..\..\lexer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\mem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\numerics.cpp
# End Source File
# Begin Source File

SOURCE=..\..\parser.cpp
# End Source File
# Begin Source File

SOURCE=..\..\prmjtime.cpp
# End Source File
# Begin Source File

SOURCE=..\..\reader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\regexpwrapper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\strings.cpp
# End Source File
# Begin Source File

SOURCE=..\..\token.cpp
# End Source File
# Begin Source File

SOURCE=..\..\utilities.cpp
# End Source File
# Begin Source File

SOURCE=..\..\world.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\bytecodecontainer.h
# End Source File
# Begin Source File

SOURCE=..\..\exception.h
# End Source File
# Begin Source File

SOURCE=..\..\formatter.h
# End Source File
# Begin Source File

SOURCE=..\..\hash.h
# End Source File
# Begin Source File

SOURCE=..\..\js2engine.h
# End Source File
# Begin Source File

SOURCE=..\..\js2metadata.h
# End Source File
# Begin Source File

SOURCE=..\..\js2value.h
# End Source File
# Begin Source File

SOURCE=..\..\lexer.h
# End Source File
# Begin Source File

SOURCE=..\..\mem.h
# End Source File
# Begin Source File

SOURCE=..\..\numerics.h
# End Source File
# Begin Source File

SOURCE=..\..\parser.h
# End Source File
# Begin Source File

SOURCE=..\..\reader.h
# End Source File
# Begin Source File

SOURCE=..\..\stlcfg.h
# End Source File
# Begin Source File

SOURCE=..\..\strings.h
# End Source File
# Begin Source File

SOURCE=..\..\token.h
# End Source File
# Begin Source File

SOURCE=..\..\utilities.h
# End Source File
# Begin Source File

SOURCE=..\..\world.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
