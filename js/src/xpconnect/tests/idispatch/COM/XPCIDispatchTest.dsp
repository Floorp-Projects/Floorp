# Microsoft Developer Studio Project File - Name="XPCIDispatchTest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=XPCIDispatchTest - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "XPCIDispatchTest.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "XPCIDispatchTest.mak" CFG="XPCIDispatchTest - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "XPCIDispatchTest - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "XPCIDispatchTest - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "XPCIDispatchTest - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Custom Build - Performing registration
OutDir=.\Debug
TargetPath=.\Debug\XPCIDispatchTest.dll
InputPath=.\Debug\XPCIDispatchTest.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "XPCIDispatchTest - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "XPCIDispatchTest___Win32_Release"
# PROP BASE Intermediate_Dir "XPCIDispatchTest___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\Release
TargetPath=.\Release\XPCIDispatchTest.dll
InputPath=.\Release\XPCIDispatchTest.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "XPCIDispatchTest - Win32 Debug"
# Name "XPCIDispatchTest - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\nsXPCDispSimple.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestArrays.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestMethods.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestNoIDispatch.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestProperties.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestScriptOff.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestScriptOn.cpp
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestWrappedJS.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\XPCIDispatchTest.cpp
# End Source File
# Begin Source File

SOURCE=.\XPCIDispatchTest.def
# End Source File
# Begin Source File

SOURCE=.\XPCIDispatchTest.idl
# ADD MTL /tlb ".\XPCIDispatchTest.tlb" /h "XPCIDispatchTest.h" /iid "XPCIDispatchTest_i.c" /Oicf
# End Source File
# Begin Source File

SOURCE=.\XPCIDispatchTest.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\nsXPCDispSimple.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestArrays.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestMethods.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestNoIDispatch.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestProperties.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestScriptOff.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestScriptOn.h
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestWrappedJS.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\XPCDispUtilities.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\nsXPCDispSimple.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestArrays.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestMethods.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestNoIDispatch.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestNoScript.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestProperties.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestScriptOff.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestScriptOn.rgs
# End Source File
# Begin Source File

SOURCE=.\nsXPCDispTestWrappedJS.rgs
# End Source File
# End Group
# Begin Group "JS Files"

# PROP Default_Filter "js"
# Begin Group "WrappedCOM"

# PROP Default_Filter ""
# Begin Group "Arrays"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Tests\WrappedCOM\Arrays\XPCIDispatchArrayTests.js
# End Source File
# End Group
# Begin Group "Attributes"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Tests\WrappedCOM\Attributes\XPCIDispatchAttributeTests.js
# End Source File
# End Group
# Begin Group "General"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Tests\WrappedCOM\General\XPCIDispatchInstantiations.js
# End Source File
# Begin Source File

SOURCE=..\Tests\WrappedCOM\General\XPCStress.js
# End Source File
# End Group
# Begin Group "Methods"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Tests\WrappedCOM\Methods\XPCIDispatchMethodTests.js
# End Source File
# End Group
# Begin Source File

SOURCE=..\Tests\WrappedCOM\shell.js
# End Source File
# End Group
# Begin Group "WrappedJS"

# PROP Default_Filter ""
# Begin Group "General (WJS)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Tests\WrappedJS\General\XPCIDispatchTestWrappedJS.js
# End Source File
# End Group
# Begin Source File

SOURCE=..\Tests\WrappedJS\shell.js
# End Source File
# End Group
# End Group
# End Target
# End Project
