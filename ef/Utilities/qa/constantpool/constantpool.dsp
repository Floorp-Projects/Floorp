# Microsoft Developer Studio Project File - Name="constantpool" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=constantpool - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "constantpool.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "constantpool.mak" CFG="constantpool - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "constantpool - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "constantpool - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "constantpool - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\winbuild\Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\dist\WINNT4.0_DBG.OBJ\include\nspr20\pr" /I "..\Utilities" /I "..\ClassReader" /I "..\Reader" /I "..\CodeGen\include" /I "..\CodeGen\common" /I "genfiles" /I "..\RegAlloc\src\regalloc" /I "..\Runtime" /I "..\PrimitiveGraph" /I "..\ClassInfo" /I "..\..\Runtime" /I "..\..\PrimitiveGraph" /I "..\..\..\dist\WINNT4.0_DBG.OBJ\include\\" /I "..\..\Utilities" /I "..\..\ClassReader" /I "..\..\ClassInfo" /I "..\..\Reader" /I "..\..\FileReader" /D "NDEBUG" /D "RELEASE" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "EF_WINDOWS" /D "DEBUG" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib ..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc20.lib /nologo /subsystem:console /debug /machine:I386 /Fixed:no
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "constantpool - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\winbuild\electric"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /w /W0 /Gm /GX /Zi /Od /I "." /I "..\..\Reader" /I "..\..\ClassInfo" /I "..\..\Runtime" /I "..\..\PrimitiveGraph" /I "..\..\..\dist\WINNT4.0_DBG.OBJ\include\\" /I "..\..\Utilities" /I "..\..\ClassReader" /I "..\..\ClassCentral" /D "_DEBUG" /D "DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "EF_WINDOWS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr20.lib ..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc20.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "constantpool - Win32 Release"
# Name "constantpool - Win32 Debug"
# Begin Group "Utilities"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Utilities\DebugUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Utilities\FloatUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Utilities\JavaBytecodes.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Utilities\JavaTypes.cpp
# End Source File
# Begin Source File

SOURCE=..\..\Utilities\Pool.cpp
# End Source File
# End Group
# Begin Group "Runtime"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Runtime\ClassWorld.cpp
# End Source File
# End Group
# Begin Group "ConstantPool"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\AttributeHandlers.cpp
# End Source File
# Begin Source File

SOURCE=.\AttributeHandlers.h
# End Source File
# Begin Source File

SOURCE=.\Attributes.h
# End Source File
# Begin Source File

SOURCE=.\ClassGen.cpp
# End Source File
# Begin Source File

SOURCE=.\ClassGen.h
# End Source File
# Begin Source File

SOURCE=.\ClassReader.cpp
# End Source File
# Begin Source File

SOURCE=.\ClassReader.h
# End Source File
# Begin Source File

SOURCE=.\ConstantPool.h
# End Source File
# Begin Source File

SOURCE=.\FileReader.cpp
# End Source File
# Begin Source File

SOURCE=.\FileReader.h
# End Source File
# Begin Source File

SOURCE=.\FileWriter.cpp
# End Source File
# Begin Source File

SOURCE=.\FileWriter.h
# End Source File
# Begin Source File

SOURCE=.\InfoItem.h
# End Source File
# Begin Source File

SOURCE=.\JavaObject.cpp
# End Source File
# End Group
# Begin Group "Reader"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\Reader\Reader.cpp
# End Source File
# End Group
# Begin Group "FileReader"

# PROP Default_Filter ""
# End Group
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# End Target
# End Project
