# Microsoft Developer Studio Project File - Name="libicalss" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libicalss - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libicalss.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libicalss.mak" CFG="libicalss - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libicalss - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libicalss - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libicalss - Win32 Release"

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
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../libical" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "YY_NO_UNISTD_H" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libicalss - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libicalss___Win32_Debug"
# PROP BASE Intermediate_Dir "libicalss___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\libical" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "YY_NO_UNISTD_H" /YX /FD /GZ /c
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

# Name "libicalss - Win32 Release"
# Name "libicalss - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\icalcalendar.c
# End Source File
# Begin Source File

SOURCE=.\icalclassify.c
# End Source File
# Begin Source File

SOURCE=.\icalcluster.c
# End Source File
# Begin Source File

SOURCE=.\icaldirset.c
# End Source File
# Begin Source File

SOURCE=.\icalfileset.c
# End Source File
# Begin Source File

SOURCE=.\icalgauge.c
# End Source File
# Begin Source File

SOURCE=.\icalmessage.c
# End Source File
# Begin Source File

SOURCE=.\icalset.c
# End Source File
# Begin Source File

SOURCE=.\icalspanlist.c
# End Source File
# Begin Source File

SOURCE=.\icalsslexer.c
# End Source File
# Begin Source File

SOURCE=.\icalssyacc.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\icalbdbset.h
# End Source File
# Begin Source File

SOURCE=.\icalbdbsetimpl.h
# End Source File
# Begin Source File

SOURCE=.\icalcalendar.h
# End Source File
# Begin Source File

SOURCE=.\icalcaputil.h
# End Source File
# Begin Source File

SOURCE=.\icalclassify.h
# End Source File
# Begin Source File

SOURCE=.\icalcluster.h
# End Source File
# Begin Source File

SOURCE=.\icalclusterimpl.h
# End Source File
# Begin Source File

SOURCE=.\icalcsdb.h
# End Source File
# Begin Source File

SOURCE=.\icaldirset.h
# End Source File
# Begin Source File

SOURCE=.\icaldirsetimpl.h
# End Source File
# Begin Source File

SOURCE=.\icalfileset.h
# End Source File
# Begin Source File

SOURCE=.\icalfilesetimpl.h
# End Source File
# Begin Source File

SOURCE=.\icalgauge.h
# End Source File
# Begin Source File

SOURCE=.\icalgaugeimpl.h
# End Source File
# Begin Source File

SOURCE=.\icalmessage.h
# End Source File
# Begin Source File

SOURCE=.\icalset.h
# End Source File
# Begin Source File

SOURCE=.\icalspanlist.h
# End Source File
# Begin Source File

SOURCE=.\icalss.h
# End Source File
# Begin Source File

SOURCE=.\icalssyacc.h
# End Source File
# End Group
# End Target
# End Project
