# Microsoft Developer Studio Project File - Name="furmon" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=furmon - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "furmon.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "furmon.mak" CFG="furmon - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "furmon - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "furmon - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "furmon - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "furmon - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "../../../Debugger/Communication" /I "../../../Utilities/Readline" /I "../../../Utilities/Disassemblers/x86" /I "../../../Utilities/General" /I "../../../../dist/WINNT4.0_DBG.OBJ/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ../../../../dist/WINNT4.0_DBG.OBJ/lib/libnspr21.lib ../../../../dist/WINNT4.0_DBG.OBJ/lib/libDebuggerChannel.lib ../../../../dist/WINNT4.0_DBG.OBJ/lib/Readline.lib kernel32.lib user32.lib imagehlp.lib /nologo /subsystem:console /debug /machine:I386 /out:"../../../../dist/WINNT4.0_DBG.OBJ/bin/furmon.exe" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "furmon - Win32 Release"
# Name "furmon - Win32 Debug"
# Begin Group "Utilities"

# PROP Default_Filter ""
# Begin Group "Disassembler"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsmTable.c
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=..\Breakpoints.cpp
# End Source File
# Begin Source File

SOURCE=..\Breakpoints.h
# End Source File
# Begin Source File

SOURCE=..\CommandLine.cpp
# End Source File
# Begin Source File

SOURCE=..\CommandLine.h
# End Source File
# Begin Source File

SOURCE=..\Commands.cpp
# End Source File
# Begin Source File

SOURCE=..\Commands.h
# End Source File
# Begin Source File

SOURCE=..\DataOutput.cpp
# End Source File
# Begin Source File

SOURCE=..\DataOutput.h
# End Source File
# Begin Source File

SOURCE=..\Debugee.cpp
# End Source File
# Begin Source File

SOURCE=..\Debugee.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Debugger\Communication\DebuggerChannel.h
# End Source File
# Begin Source File

SOURCE=..\main.cpp
# End Source File
# Begin Source File

SOURCE=..\MonitorExpression.cpp
# End Source File
# Begin Source File

SOURCE=..\MonitorExpression.h
# End Source File
# Begin Source File

SOURCE=..\Symbols.cpp
# End Source File
# Begin Source File

SOURCE=..\Symbols.h
# End Source File
# Begin Source File

SOURCE=..\Win32Util.cpp
# End Source File
# Begin Source File

SOURCE=..\Win32Util.h
# End Source File
# End Target
# End Project
