# Microsoft Developer Studio Project File - Name="DebuggerChannel" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=DebuggerChannel - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "DebuggerChannel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "DebuggerChannel.mak" CFG="DebuggerChannel - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DebuggerChannel - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "DebuggerChannel - Win32 Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "DebuggerChannel - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Debugger"
# PROP BASE Intermediate_Dir "Debugger"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\Exports" /I "..\..\..\Exports\md" /I "..\..\..\Utilities\General" /I "..\..\..\Utilities\General\md" /I "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\Runtime\System\md\x86" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr21.lib ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc21_s.lib wsock32.lib kernel32.lib user32.lib /nologo /subsystem:windows /dll /machine:I386

!ELSEIF  "$(CFG)" == "DebuggerChannel - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "electric"
# PROP Intermediate_Dir "electric"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\..\Exports" /I "..\..\..\Exports\md" /I "..\..\..\Utilities\General" /I "..\..\..\Utilities\General\md" /I "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\Runtime\System\md\x86" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr21.lib ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc21_s.lib wsock32.lib kernel32.lib user32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "DebuggerChannel - Win32 Release"
# Name "DebuggerChannel - Win32 Debug"
# Begin Source File

SOURCE=..\..\..\Debugger\Communication\DebuggerChannel.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Debugger\Communication\DebuggerChannel.h
# End Source File
# End Target
# End Project
