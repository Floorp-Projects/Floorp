# Microsoft Developer Studio Project File - Name="MozillaControl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=MozillaControl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MozillaControl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MozillaControl.mak" CFG="MozillaControl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MozillaControl - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "MozillaControl - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "MozillaControl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "MozillaControl.exe"
# PROP BASE Bsc_Name "MozillaControl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f makefile.win"
# PROP Rebuild_Opt "/a"
# PROP Target_File "win32_o.obj\mozctl.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "MozillaControl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f MozillaControl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "MozillaControl.exe"
# PROP BASE Bsc_Name "MozillaControl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /f makefile.win"
# PROP Rebuild_Opt "/a"
# PROP Target_File "M:\source\mozilla\dist\WIN32_D.OBJ\bin\mozctl.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "MozillaControl - Win32 Release"
# Name "MozillaControl - Win32 Debug"

!IF  "$(CFG)" == "MozillaControl - Win32 Release"

!ELSEIF  "$(CFG)" == "MozillaControl - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ActiveScriptSite.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlSite.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlSiteIPFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\DropTarget.cpp
# End Source File
# Begin Source File

SOURCE=.\guids.cpp
# End Source File
# Begin Source File

SOURCE=.\HelperAppDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlDocument.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElement.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElementCollection.cpp
# End Source File
# Begin Source File

SOURCE=.\IEHtmlNode.cpp
# End Source File
# Begin Source File

SOURCE=.\ItemContainer.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.cpp
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.idl
# End Source File
# Begin Source File

SOURCE=.\nsSetupRegistry.cpp
# End Source File
# Begin Source File

SOURCE=.\PromptService.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertyBag.cpp
# End Source File
# Begin Source File

SOURCE=.\PropertyDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\TextProviderService.cpp
# End Source File
# Begin Source File

SOURCE=.\WebBrowserContainer.cpp
# End Source File
# Begin Source File

SOURCE=.\WindowCreator.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ActiveScriptSite.h
# End Source File
# Begin Source File

SOURCE=.\ActiveXTypes.h
# End Source File
# Begin Source File

SOURCE=.\BrowserDiagnostics.h
# End Source File
# Begin Source File

SOURCE=.\ControlSite.h
# End Source File
# Begin Source File

SOURCE=.\ControlSiteIPFrame.h
# End Source File
# Begin Source File

SOURCE=.\CPMozillaControl.h
# End Source File
# Begin Source File

SOURCE=.\DHTMLCmdIds.h
# End Source File
# Begin Source File

SOURCE=.\DropTarget.h
# End Source File
# Begin Source File

SOURCE=.\guids.h
# End Source File
# Begin Source File

SOURCE=.\HelperAppDlg.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlDocument.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElement.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlElementCollection.h
# End Source File
# Begin Source File

SOURCE=.\IEHtmlNode.h
# End Source File
# Begin Source File

SOURCE=.\IOleCommandTargetImpl.h
# End Source File
# Begin Source File

SOURCE=.\ItemContainer.h
# End Source File
# Begin Source File

SOURCE=.\IWebBrowserImpl.h
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.h
# End Source File
# Begin Source File

SOURCE=.\PromptService.h
# End Source File
# Begin Source File

SOURCE=.\PropertyBag.h
# End Source File
# Begin Source File

SOURCE=.\PropertyDlg.h
# End Source File
# Begin Source File

SOURCE=.\PropertyList.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TextProviderService.h
# End Source File
# Begin Source File

SOURCE=.\WebBrowserContainer.h
# End Source File
# Begin Source File

SOURCE=.\WindowCreator.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\MozillaBrowser.ico
# End Source File
# Begin Source File

SOURCE=.\MozillaBrowser.rgs
# End Source File
# Begin Source File

SOURCE=.\MozillaControl.rc
# End Source File
# End Group
# Begin Source File

SOURCE=.\Makefile.in
# End Source File
# End Target
# End Project
