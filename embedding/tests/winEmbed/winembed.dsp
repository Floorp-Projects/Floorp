# Microsoft Developer Studio Project File - Name="winembed" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=winembed - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "winembed.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "winembed.mak" CFG="winembed - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "winembed - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "winembed - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "winembed - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f winembed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "winembed.exe"
# PROP BASE Bsc_Name "winembed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f makefile.win"
# PROP Rebuild_Opt "/a"
# PROP Target_File "win32_o.obj\winembed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "winembed - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f winembed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "winembed.exe"
# PROP BASE Bsc_Name "winembed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /f makefile.win"
# PROP Rebuild_Opt "/a"
# PROP Target_File "win32_d.obj\winembed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "winembed - Win32 Release"
# Name "winembed - Win32 Debug"

!IF  "$(CFG)" == "winembed - Win32 Release"

!ELSEIF  "$(CFG)" == "winembed - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\WebBrowserChrome.cpp
# End Source File
# Begin Source File

SOURCE=.\WindowCreator.cpp
# End Source File
# Begin Source File

SOURCE=.\winEmbed.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\WebBrowserChrome.h
# End Source File
# Begin Source File

SOURCE=.\WindowCreator.h
# End Source File
# Begin Source File

SOURCE=.\winEmbed.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\SMALL.ICO
# End Source File
# Begin Source File

SOURCE=.\winEmbed.ICO
# End Source File
# Begin Source File

SOURCE=.\winEmbed.rc
# End Source File
# End Group
# End Target
# End Project
