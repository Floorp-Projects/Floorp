# Microsoft Developer Studio Project File - Name="javah" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=javah - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "javah.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "javah.mak" CFG="javah - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "javah - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "javah - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "javah - Win32 Release"

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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\Debugger ..\..\..\Exports ..\..\..\Exports\md" /I "..\..\..\Exports ..\..\..\Exports\md" /I "..\..\..\Debugger" /I "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\Driver\StandAloneJava" /I "..\..\..\Runtime\System" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\Compiler\Optimizer" /I "..\..\..\Utilities\General" /I "..\..\..\Exports" /I "..\..\..\Exports\md" /I "..\..\..\Runtime\System\md" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "XP_PC" /D "USE_PR_IO" /D "IMPORTING_VM_FILES" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 release\electricalfire.lib ..\..\..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr21.lib ..\..\..\..\dist\WINNT4.0_OPT.OBJ\lib\libplc20_s.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /incremental:yes /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "javah - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "electric"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /Zi /I "..\..\..\Debugger\Communication" /I "..\..\..\Debugger" /I "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\Driver\StandAloneJava" /I "..\..\..\Runtime\System" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\Compiler\Optimizer" /I "..\..\..\Utilities\General" /I "..\..\..\Exports" /I "..\..\..\Exports\md" /I "..\..\..\Runtime\System\md" /D "DEBUG" /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "DEBUG_DESANTIS" /D "DEBUG_LOG" /D "DEBUG_kini" /D "XP_PC" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "USE_PR_IO" /D "IMPORTING_VM_FILES" /FR"electric/" /Fo"electric/" /Fd"electric/" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 electric\electricalfire.lib ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr21.lib ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc21_s.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "javah - Win32 Release"
# Name "javah - Win32 Debug"
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\HeaderGenerator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\HeaderGenerator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\JNIHeaderGenerator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\JNIHeaderGenerator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\main.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\NetscapeHeaderGenerator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Tools\JavaH\NetscapeHeaderGenerator.h
# End Source File
# End Target
# End Project
