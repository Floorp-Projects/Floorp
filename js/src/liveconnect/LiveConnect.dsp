# Microsoft Developer Studio Project File - Name="LiveConnect" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=LiveConnect - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "LiveConnect.mak".
!MESSAGE 
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

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Zi /O2 /I ".." /I "$(JDK)\include" /I "$(JDK)\include\win32" /D "NDEBUG" /D "XP_PC" /D "JSFILE" /D "WIN32" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\Release\js32.lib $(JDK)\lib\javai.lib /nologo /subsystem:windows /dll /debug /machine:I386

!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I ".." /I "$(JDK)\include" /I "$(JDK)\include\win32" /D "_DEBUG" /D "_CONSOLE" /D "DEBUG" /D "XP_PC" /D "JSFILE" /D "WIN32" /D "_WINDOWS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\Debug\js32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Debug/LiveConnect_g.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "LiveConnect - Win32 Release"
# Name "LiveConnect - Win32 Debug"
# Begin Source File

SOURCE=..\liveconnect\jsj.c
# End Source File
# Begin Source File

SOURCE=..\liveconnect\jsj_array.c
# End Source File
# Begin Source File

SOURCE=..\liveconnect\jsj_class.c
# End Source File
# Begin Source File

SOURCE=..\liveconnect\jsj_convert.c
# End Source File
# Begin Source File

SOURCE=..\liveconnect\jsj_field.c
# End Source File
# Begin Source File

SOURCE=.\jsj_hash.c
# End Source File
# Begin Source File

SOURCE=.\jsj_JavaArray.c
# End Source File
# Begin Source File

SOURCE=.\jsj_JavaClass.c
# End Source File
# Begin Source File

SOURCE=.\jsj_JavaMember.c
# End Source File
# Begin Source File

SOURCE=.\jsj_JavaObject.c
# End Source File
# Begin Source File

SOURCE=.\jsj_JavaPackage.c
# End Source File
# Begin Source File

SOURCE=.\jsj_JSObject.c

!IF  "$(CFG)" == "LiveConnect - Win32 Release"

!ELSEIF  "$(CFG)" == "LiveConnect - Win32 Debug"

# ADD CPP /I ".\_jni"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\liveconnect\jsj_method.c
# End Source File
# Begin Source File

SOURCE=..\liveconnect\jsj_utils.c
# End Source File
# End Target
# End Project
