# Microsoft Developer Studio Project File - Name="testembed" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=testembed - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "testembed.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "testembed.mak" CFG="testembed - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "testembed - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "testembed - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "testembed - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f testembed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "testembed.exe"
# PROP BASE Bsc_Name "testembed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f "makfile.win""
# PROP Rebuild_Opt "/a"
# PROP Target_File "WIN32_O.OBJ\testembed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testembed - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f testembed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "testembed.exe"
# PROP BASE Bsc_Name "testembed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /f "makefile.win""
# PROP Rebuild_Opt "/a"
# PROP Target_File "WIN32_D.OBJ\testembed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "testembed - Win32 Release"
# Name "testembed - Win32 Debug"

!IF  "$(CFG)" == "testembed - Win32 Release"

!ELSEIF  "$(CFG)" == "testembed - Win32 Debug"

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

SOURCE=.\BrowserImplHistoryLstnr.cpp
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

SOURCE=.\DomWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\MostRecentUrls.cpp
# End Source File
# Begin Source File

SOURCE=.\nsiDirServ.cpp
# End Source File
# Begin Source File

SOURCE=.\nsiHistory.cpp
# End Source File
# Begin Source File

SOURCE=.\nsIRequest.cpp
# End Source File
# Begin Source File

SOURCE=.\nsIWebNav.cpp
# End Source File
# Begin Source File

SOURCE=.\nsProfile.cpp
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

SOURCE=.\QaUtils.cpp
# End Source File
# Begin Source File

SOURCE=.\Selection.cpp
# End Source File
# Begin Source File

<<<<<<< testembed.dsp
SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

=======
SOURCE=.\TestCallbacks.cpp
# End Source File
# Begin Source File

>>>>>>> 1.3
SOURCE=.\TestEmbed.cpp
# End Source File
# Begin Source File

SOURCE=.\testembed.rc
# End Source File
# Begin Source File

SOURCE=.\Tests.cpp
# End Source File
# Begin Source File

SOURCE=.\UrlDialog.cpp
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

SOURCE=.\DomWindow.h
# End Source File
# Begin Source File

SOURCE=.\IBrowserFrameGlue.h
# End Source File
# Begin Source File

SOURCE=.\MostRecentUrls.h
# End Source File
# Begin Source File

SOURCE=.\nsiDirServ.h
# End Source File
# Begin Source File

SOURCE=.\nsiHistory.h
# End Source File
# Begin Source File

SOURCE=.\nsIRequest.h
# End Source File
# Begin Source File

SOURCE=.\nsIWebNav.h
# End Source File
# Begin Source File

SOURCE=.\nsProfile.h
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

SOURCE=.\QaUtils.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\Selection.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TestEmbed.h
# End Source File
# Begin Source File

SOURCE=.\Tests.h
# End Source File
# Begin Source File

SOURCE=.\UrlDialog.h
# End Source File
# Begin Source File

SOURCE=.\winEmbedFileLocProvider.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\testembed.ico
# End Source File
# Begin Source File

SOURCE=.\res\testembed.ico
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# End Group
# End Target
# End Project
