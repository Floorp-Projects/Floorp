# Microsoft Developer Studio Project File - Name="Package" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Package - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Package.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Package.mak" CFG="Package - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Package - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Package - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Package - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\..\Debugger\Communication" /I "..\..\..\..\Driver\StandAloneJava" /I "..\..\..\..\Utilities\General" /I "..\genfiles" /I "..\..\..\..\Compiler\FrontEnd" /I "..\..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\..\Runtime\System" /I "..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\..\Utilities\zlib" /I "..\..\..\..\Runtime\ClassInfo" /I "..\..\..\..\Runtime\ClassReader" /I "..\..\..\..\Runtime\FileReader" /I "..\..\..\..\Runtime\NativeMethods" /I "..\..\..\..\Compiler\CodeGenerator" /I "..\..\..\..\Compiler\RegisterAllocator" /I "..\..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\..\Compiler\Optimizer" /I "..\..\..\..\Packages\java\lang\nativesrc\geninclude" /I "..\..\..\..\Packages\java\io\nativesrc\geninclude" /I "..\..\..\..\Packages\java\security\nativesrc\geninclude" /I "..\..\..\..\Debugger" /I "..\..\..\..\Exports" /I "..\..\..\..\Exports\md" /I "..\..\..\..\Runtime\System\md\x86" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "_CONSOLE" /D "_MBCS" /D "XP_PC" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /dll /machine:I386
# ADD LINK32 ..\release\electricalfire.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr21.lib ..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc21_s.lib wsock32.lib /nologo /subsystem:console /dll /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "Package - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\electric"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /GX /Zi /I "..\..\..\..\Debugger\Communication" /I "..\..\..\..\Driver\StandAloneJava" /I "..\..\..\..\Utilities\General" /I "..\genfiles" /I "..\..\..\..\Compiler\FrontEnd" /I "..\..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\..\Runtime\System" /I "..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\..\Utilities\zlib" /I "..\..\..\..\Runtime\ClassInfo" /I "..\..\..\..\Runtime\ClassReader" /I "..\..\..\..\Runtime\FileReader" /I "..\..\..\..\Runtime\NativeMethods" /I "..\..\..\..\Compiler\CodeGenerator" /I "..\..\..\..\Compiler\RegisterAllocator" /I "..\..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\..\Compiler\Optimizer" /I "..\..\..\..\Packages\java\lang\nativesrc\geninclude" /I "..\..\..\..\Packages\java\io\nativesrc\geninclude" /I "..\..\..\..\Packages\java\security\nativesrc\geninclude" /I "..\..\..\..\Debugger" /I "..\..\..\..\Exports" /I "..\..\..\..\Exports\md" /I "..\..\..\..\Runtime\System\md\x86" /D "DEBUG" /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "DEBUG_DESANTIS" /D "DEBUG_kini" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "XP_PC" /D "IMPORTING_VM_FILES" /FR"electric/" /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\electric\electricalfire.lib kernel32.lib user32.lib ..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr21.lib ..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc21_s.lib wsock32.lib /nologo /subsystem:console /dll /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "Package - Win32 Release"
# Name "Package - Win32 Debug"
# Begin Group "java"

# PROP Default_Filter ""
# Begin Group "lang"

# PROP Default_Filter ""
# Begin Group "reflect"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\reflect\Array.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\reflect\Constructor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\reflect\Field.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\reflect\Method.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Character.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Class.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\ClassLoader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Compiler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Double.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Float.cpp
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_ArithmeticException.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Character.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Class.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_ClassLoader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Compiler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Double.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Exception.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Float.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Math.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Number.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Runtime.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_RuntimeException.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_SecurityManager.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Throwable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Math.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Object.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Runtime.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\SecurityManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\String.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\System.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Thread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\lang\nativesrc\Throwable.cpp
# End Source File
# End Group
# Begin Group "io"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\File.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\FileDescriptor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\FileInputStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\FileOutputStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_File.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_FileDescriptor.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_FileInputStream.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_FileOutputStream.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_FilterOutputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_InputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_OutputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_PrintStream.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_RandomAccessFile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\geninclude\java_lang_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\io\nativesrc\Mapping.h
# End Source File
# End Group
# Begin Group "security"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\Packages\java\security\nativesrc\AccessController.cpp
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\security\nativesrc\geninclude\java_security_AccessController.h
# End Source File
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\security\nativesrc\geninclude\java_security_ProtectionDomain.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\security\nativesrc\ProtectionDomain.cpp
# End Source File
# End Group
# Begin Group "util"

# PROP Default_Filter ""
# Begin Source File

SOURCE=\
..\..\..\..\Packages\java\util\nativesrc\geninclude\java_util_ResourceBundle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\Packages\java\util\nativesrc\ResourceBundle.cpp
# ADD CPP /I "..\..\..\..\Packages\java\util\nativesrc\geninclude"
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=..\..\..\..\Packages\rt.jar

!IF  "$(CFG)" == "Package - Win32 Release"

!ELSEIF  "$(CFG)" == "Package - Win32 Debug"

# Begin Custom Build - Generating C header files from Java sources
InputPath=..\..\..\..\Packages\rt.jar

BuildCmds= \
	..\electric\javah -classpath ../../../../Packages/rt.jar -d\
             ../../../../Packages/java/security/nativesrc/geninclude\
                java/security/AccessController java/security/ProtectionDomain \
	..\electric\javah -classpath ../../../../Packages/rt.jar -d\
                                        ../../../../Packages/java/lang/nativesrc/geninclude\
                                        java/lang/ArithmeticException java/lang/Character\
                                        java/lang/Class java/lang/ClassLoader\
                                        java/lang/Compiler java/lang/Double java/lang/Float \
	..\electric\javah -classpath ../../../../Packages/rt.jar -d\
                                ../../../../Packages/java/lang/nativesrc/geninclude\
                                java/lang/Math\
                                java/lang/Runtime\
                                java/lang/SecurityManager\
                                java/lang/System java/lang/String\
                                java/lang/Thread \
	..\electric\javah -classpath ../../../../Packages/rt.jar -d\
                                ../../../../Packages/java/lang/nativesrc/reflect/geninclude\
                                java/lang/reflect/Field\
                                java/lang/reflect/Method\
                                java/lang/reflect/Constructor\
                                java/lang/reflect/Array \
	..\electric\javah -classpath ../../../../Packages/rt.jar -d\
                                ../../../../Packages/java/io/nativesrc/geninclude java/io/File\
                                java/io/FileDescriptor\
                                java/io/FileInputStream\
                                java/io/FileOutputStream\
                                java/io/PrintStream java/io/RandomAccessFile \
	..\electric\javah -classpath ../../../../Packages/rt.jar -d\
                                ../../../../Packages/java/util/nativesrc/geninclude\
                                java/util/ResourceBundle  java/util/Properties	

"..\..\..\..\Packages\java\security\nativesrc\geninclude\ProtectionDomain.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\security\nativesrc\geninclude\java_lang_Object.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Class.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_ClassLoader.h" :\
 $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Compiler.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Double.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Float.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Math.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Object.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Runtime.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_System.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_String.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Thread.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Number.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Ref.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_lang_Exception.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\lang\nativesrc\geninclude\java_util_properties.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_File.h" : $(SOURCE)\
 "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_FileDescriptor.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\io\nativesrc\geninclude\java_lang_Object.h" : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"..\..\..\..\Packages\java\io\nativesrc\geninclude\java_io_FileInputStream.h" :\
 $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
