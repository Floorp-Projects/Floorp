# Microsoft Developer Studio Project File - Name="Burg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=Burg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Burg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Burg.mak" CFG="Burg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Burg - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Burg - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Burg - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "." /I "..\..\..\..\Tools\burg" /I "..\..\..\Exports ..\..\..\Exports\md" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# SUBTRACT BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "." /I "..\..\..\..\Tools\burg\\" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "DEBUG" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# SUBTRACT BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Burg - Win32 Release"
# Name "Burg - Win32 Debug"
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\b.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\be.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\burs.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\closure.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\delta.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\fe.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\fe.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\gram.y

!IF  "$(CFG)" == "Burg - Win32 Release"

# Begin Custom Build - Performing Custom-Build Setup
InputPath=..\..\..\..\Tools\Burg\gram.y

BuildCmds= \
	$(NSTOOLS)\bin\yacc -l -b y.tab -d $(InputPath)

"y.tab.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"y.tab.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "Burg - Win32 Debug"

# Begin Custom Build - Performing Custom-Build Setup
InputPath=..\..\..\..\Tools\Burg\gram.y

BuildCmds= \
	..\..\..\..\Tools\bin\bison -l -d $(InputPath)

"y.tab.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"y.tab.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\item.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\lex.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\list.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\main.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\map.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\nonterminal.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\operator.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\pattern.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\plank.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\queue.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\rule.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\string.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\symtab.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\table.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\test.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\trim.c
# End Source File
# Begin Source File

SOURCE=.\y.tab.c
# End Source File
# Begin Source File

SOURCE=.\y.tab.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Tools\Burg\zalloc.c
# End Source File
# End Target
# End Project
