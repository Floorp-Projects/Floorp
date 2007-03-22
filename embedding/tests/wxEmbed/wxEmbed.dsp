# Microsoft Developer Studio Project File - Name="wxEmbed" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=wxEmbed - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "wxEmbed.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "wxEmbed.mak" CFG="wxEmbed - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "wxEmbed - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "wxEmbed - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "wxEmbed - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f wxEmbed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "wxEmbed.exe"
# PROP BASE Bsc_Name "wxEmbed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f makefile.vc"
# PROP Rebuild_Opt "/a"
# PROP Target_File "c:\m\source_1.4\mozilla\dist\bin"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "wxEmbed - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "wxEmbed___Win32_Debug"
# PROP BASE Intermediate_Dir "wxEmbed___Win32_Debug"
# PROP BASE Cmd_Line "NMAKE /f wxEmbed.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "wxEmbed.exe"
# PROP BASE Bsc_Name "wxEmbed.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "wxEmbed___Win32_Debug"
# PROP Intermediate_Dir "wxEmbed___Win32_Debug"
# PROP Cmd_Line "nmake /f makefile.vc"
# PROP Rebuild_Opt "/a"
# PROP Target_File "c:\m\source_1.4\mozilla\dist\bin\wxEmbed.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "wxEmbed - Win32 Release"
# Name "wxEmbed - Win32 Debug"

!IF  "$(CFG)" == "wxEmbed - Win32 Release"

!ELSEIF  "$(CFG)" == "wxEmbed - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\BrowserFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\ChatFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\EditorFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\EmbedApp.cpp
# End Source File
# Begin Source File

SOURCE=.\GeckoContainer.cpp
# End Source File
# Begin Source File

SOURCE=.\GeckoContainerUI.cpp
# End Source File
# Begin Source File

SOURCE=.\GeckoFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\GeckoProtocolHandler.cpp
# End Source File
# Begin Source File

SOURCE=.\GeckoWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\GeckoWindowCreator.cpp
# End Source File
# Begin Source File

SOURCE=.\MailFrame.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BrowserFrame.h
# End Source File
# Begin Source File

SOURCE=.\ChatFrame.h
# End Source File
# Begin Source File

SOURCE=.\EditorFrame.h
# End Source File
# Begin Source File

SOURCE=.\GeckoContainer.h
# End Source File
# Begin Source File

SOURCE=.\GeckoFrame.h
# End Source File
# Begin Source File

SOURCE=.\GeckoProtocolHandler.h
# End Source File
# Begin Source File

SOURCE=.\GeckoWindow.h
# End Source File
# Begin Source File

SOURCE=.\GeckoWindowCreator.h
# End Source File
# Begin Source File

SOURCE=.\global.h
# End Source File
# Begin Source File

SOURCE=.\MailFrame.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Group "Image Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\rc\aligncenter.gif
# End Source File
# Begin Source File

SOURCE=.\rc\alignleft.gif
# End Source File
# Begin Source File

SOURCE=.\rc\alignright.gif
# End Source File
# Begin Source File

SOURCE=.\rc\anchor.gif
# End Source File
# Begin Source File

SOURCE=.\rc\back.gif
# End Source File
# Begin Source File

SOURCE=.\rc\background.gif
# End Source File
# Begin Source File

SOURCE=.\rc\bold.gif
# End Source File
# Begin Source File

SOURCE=.\rc\decreasefont.gif
# End Source File
# Begin Source File

SOURCE=.\rc\forward.gif
# End Source File
# Begin Source File

SOURCE=.\rc\help.gif
# End Source File
# Begin Source File

SOURCE=.\rc\home.gif
# End Source File
# Begin Source File

SOURCE=.\rc\increasefont.gif
# End Source File
# Begin Source File

SOURCE=.\rc\indent.gif
# End Source File
# Begin Source File

SOURCE=.\rc\italic.gif
# End Source File
# Begin Source File

SOURCE=.\rc\outdent.gif
# End Source File
# Begin Source File

SOURCE=.\rc\pen.gif
# End Source File
# Begin Source File

SOURCE=.\rc\reload.gif
# End Source File
# Begin Source File

SOURCE=.\rc\stop.gif
# End Source File
# Begin Source File

SOURCE=.\rc\underline.gif
# End Source File
# End Group
# Begin Source File

SOURCE=.\rc\browser.xrc
# End Source File
# Begin Source File

SOURCE=.\rc\chat.xrc
# End Source File
# Begin Source File

SOURCE=.\rc\editor.xrc
# End Source File
# Begin Source File

SOURCE=.\rc\mail.xrc
# End Source File
# Begin Source File

SOURCE=.\wxEmbed.rc
# End Source File
# End Group
# Begin Group "Generated Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\resource.cpp
# End Source File
# End Group
# Begin Group "External Files"

# PROP Default_Filter ""
# End Group
# Begin Source File

SOURCE=.\makefile.vc
# End Source File
# Begin Source File

SOURCE=.\README.txt
# End Source File
# End Target
# End Project
