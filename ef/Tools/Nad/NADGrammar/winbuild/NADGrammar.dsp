# Microsoft Developer Studio Project File - Name="NADGrammar" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=NADGrammar - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NADGrammar.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NADGrammar.mak" CFG="NADGrammar - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NADGrammar - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "NADGrammar - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I ".." /I "..\..\..\Debugger ..\..\..\Exports ..\..\..\Exports\md" /I "..\..\..\Exports ..\..\..\Exports\md" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

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
# ADD CPP /nologo /ML /W3 /Gm /GX /Zi /Od /I ".." /I "..\..\..\Debugger" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"MSVCRT.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none /nodefaultlib

!ENDIF 

# Begin Target

# Name "NADGrammar - Win32 Release"
# Name "NADGrammar - Win32 Debug"
# Begin Group "Generated Files (Debug)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Debug\lex.yy.c

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Debug\nadgrammar.tab.c

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Debug\nadgrammar.tab.h

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Generated Files (Release)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Release\lex.yy.c

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Release\nadgrammar.tab.c

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\Release\nadgrammar.tab.h

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\alloca.c
# End Source File
# Begin Source File

SOURCE=..\NADGrammar.c
# End Source File
# Begin Source File

SOURCE=..\NADGrammar.l

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

# Begin Custom Build - Flex'ing my muscles
OutDir=.\Release
InputPath=..\NADGrammar.l

"$(OutDir)\lex.yy.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(NSTOOLS)\bin\flex -t $(InputPath) > $(OutDir)\lex.yy.c

# End Custom Build

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

# Begin Custom Build - flex
OutDir=.\Debug
InputPath=..\NADGrammar.l

"$(OutDir)\lex.yy.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(MOZ_TOOLS)\bin\flex -t $(InputPath) > $(OutDir)\lex.yy.c

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\NADGrammar.y

!IF  "$(CFG)" == "NADGrammar - Win32 Release"

# Begin Custom Build - Bison me
OutDir=.\Release
InputPath=..\NADGrammar.y

"$(OutDir)\NADGrammar.tab.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(NSTOOLS)\bin\bison.exe --defines $(InputPath) --output\
      $(OutDir)\NADGrammar.tab.c

# End Custom Build

!ELSEIF  "$(CFG)" == "NADGrammar - Win32 Debug"

# Begin Custom Build - bison me
OutDir=.\Debug
InputPath=..\NADGrammar.y

"$(OutDir)\NADGrammar.tab.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(MOZ_TOOLS)\bin\bison.exe --defines $(InputPath) --output\
      $(OutDir)\NADGrammar.tab.c

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\NADGrammarTypes.c
# End Source File
# Begin Source File

SOURCE=..\NADGrammarTypes.h
# End Source File
# Begin Source File

SOURCE=..\NADOutput.c
# End Source File
# Begin Source File

SOURCE=..\NADOutput.h
# End Source File
# End Target
# End Project
