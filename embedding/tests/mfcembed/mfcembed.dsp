# Microsoft Developer Studio Project File - Name="mfcembed" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=mfcembed - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mfcembed.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mfcembed.mak" CFG="mfcembed - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mfcembed - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "mfcembed - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "mfcembed - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f mfcembed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "mfcembed.exe"
# PROP BASE Bsc_Name "mfcembed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f "makfile.win""
# PROP Rebuild_Opt "/a"
# PROP Target_File "WIN32_O.OBJ\mfcembed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "mfcembed - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f mfcembed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "mfcembed.exe"
# PROP BASE Bsc_Name "mfcembed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /f "makefile.win""
# PROP Rebuild_Opt "/a"
# PROP Target_File "WIN32_D.OBJ\mfcembed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "mfcembed - Win32 Release"
# Name "mfcembed - Win32 Debug"

!IF  "$(CFG)" == "mfcembed - Win32 Release"

!ELSEIF  "$(CFG)" == "mfcembed - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\BrowserFrameGlue.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowserFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowserImpl.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowserImplCtxMenuLstnr.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowserImplPrompt.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowserImplWebPrgrsLstnr.cpp
# End Source File
# Begin Source File

SOURCE=.\BrowserView.cpp
# End Source File
# Begin Source File

SOURCE=.\Dialogs.cpp
# End Source File
# Begin Source File

SOURCE=.\MfcEmbed.cpp
# End Source File
# Begin Source File

SOURCE=.\MfcEmbed.rc
# End Source File
# Begin Source File

SOURCE=.\MostRecentUrls.cpp
# End Source File
# Begin Source File

SOURCE=.\Preferences.cpp
# End Source File
# Begin Source File

SOURCE=.\PrintProgressDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ProfileMgr.cpp
# End Source File
# Begin Source File

SOURCE=.\ProfilesDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\SecurityInfoDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\winEmbedFileLocProvider.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BrowserFrm.h
# End Source File
# Begin Source File

SOURCE=.\BrowserImpl.h
# End Source File
# Begin Source File

SOURCE=.\BrowserView.h
# End Source File
# Begin Source File

SOURCE=.\Dialogs.h
# End Source File
# Begin Source File

SOURCE=.\IBrowserFrameGlue.h
# End Source File
# Begin Source File

SOURCE=.\MfcEmbed.h
# End Source File
# Begin Source File

SOURCE=.\MostRecentUrls.h
# End Source File
# Begin Source File

SOURCE=.\Preferences.h
# End Source File
# Begin Source File

SOURCE=.\PrintProgressDialog.h
# End Source File
# Begin Source File

SOURCE=.\ProfileMgr.h
# End Source File
# Begin Source File

SOURCE=.\ProfilesDlg.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SecurityInfoDlg.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\winEmbedFileLocProvider.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\broken.ico
# End Source File
# Begin Source File

SOURCE=.\res\mfcembed.ico
# End Source File
# Begin Source File

SOURCE=.\res\sinsecur.ico
# End Source File
# Begin Source File

SOURCE=.\res\ssecur.ico
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# End Group
# End Target
# End Project
