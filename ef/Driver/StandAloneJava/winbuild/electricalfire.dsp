# Microsoft Developer Studio Project File - Name="electricalfire" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=electricalfire - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "electricalfire.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "electricalfire.mak" CFG="electricalfire - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "electricalfire - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "electricalfire - Win32 Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "electricalfire - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\Exports" /I "..\..\..\Exports\md" /I "..\..\..\Utilities\General" /I "..\..\..\Utilities\General\md" /I "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\Runtime\System\md\x86" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "NDEBUG" /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "XP_PC" /D "DLL_BUILD" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "GENERATE_NATIVE_STUBS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /dll /machine:I386
# ADD LINK32 electric\DebuggerChannel.lib ..\..\..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib ..\..\..\..\dist\WINNT4.0_OPT.OBJ\lib\libplc20_s.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /dll /machine:I386 /nodefaultlib:"MSVCRT"
# SUBTRACT LINK32 /incremental:yes /debug

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "electric"
# PROP BASE Intermediate_Dir "electric"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "electric"
# PROP Intermediate_Dir "electric"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /I "..\..\..\Exports" /I "..\..\..\Exports\md" /I "..\..\..\Utilities\General" /I "..\..\..\Utilities\DisAssemblers\x86" /I "..\..\..\Utilities\General\md" /I "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I "..\..\..\Runtime\System\md\x86" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I "..\..\..\Compiler\CodeGenerator\md\x86" /I "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "DEBUG" /D "DEBUG_LOG" /D "_DEBUG" /D "DEBUG_DESANTIS" /D "DEBUG_kini" /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "XP_PC" /D "DLL_BUILD" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "GENERATE_NATIVE_STUBS" /FR /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 electric\DebuggerChannel.lib ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libnspr21.lib ..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib\libplc21_s.lib wsock32.lib kernel32.lib user32.lib /nologo /subsystem:console /dll /debug /machine:I386 /nodefaultlib:"MSVCRT" /pdbtype:sept /libpath:"..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "electricalfire - Win32 Release"
# Name "electricalfire - Win32 Debug"
# Begin Group "GeneratedFiles"

# PROP Default_Filter ""
# Begin Group "x86 generated files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\genfiles\x86-win32.nad.burg.cpp"
# End Source File
# Begin Source File

SOURCE=".\genfiles\x86-win32.nad.burg.h"
# End Source File
# End Group
# Begin Group "ppc generated files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\genfiles\ppc601-macos.nad.burg.cpp"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\genfiles\ppc601-macos.nad.burg.h"
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Source File

SOURCE=.\genfiles\PrimitiveOperations.h
# End Source File
# End Group
# Begin Group "Runtime"

# PROP Default_Filter ""
# Begin Group "ClassInfo"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\ClassCentral.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\ClassCentral.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\ClassFileSummary.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\ClassFileSummary.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\Collector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\PathComponent.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\PathComponent.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassInfo\StringPool.h
# End Source File
# End Group
# Begin Group "ClassReader"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\AttributeHandlers.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\AttributeHandlers.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\Attributes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\ClassReader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\ClassReader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\ConstantPool.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\InfoItem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\ClassReader\utils.h
# End Source File
# End Group
# Begin Group "FileReader"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\BufferedFileReader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\BufferedFileReader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\DiskFileReader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\DiskFileReader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\FileReader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\FileReader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\ZipArchive.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\ZipArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\ZipFileReader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\FileReader\ZipFileReader.h
# End Source File
# End Group
# Begin Group "NativeMethods"

# PROP Default_Filter ""
# Begin Group "md No. 2"

# PROP Default_Filter ""
# Begin Group "x86 No. 1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\md\x86\x86NativeMethodStubs.cpp
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\Jni.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\JNIManglers.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\JNIManglers.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\JniRuntime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NameMangler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NameMangler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NativeMethodStubs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NetscapeManglers.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\NativeMethods\NetscapeManglers.h
# End Source File
# End Group
# Begin Group "System"

# PROP Default_Filter ""
# Begin Group "md_"

# PROP Default_Filter ""
# Begin Group "x86_"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\Win32ExceptionHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86ExceptionHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86Win32InvokeNative.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86Win32Thread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\x86\x86Win32Thread.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Runtime\System\md\FieldOrMethod_md.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Runtime\System\ClassWorld.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\ClassWorld.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Exceptions.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\FieldOrInterfaceSummary.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\FieldOrMethod.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\FieldOrMethod.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\InterfaceDispatchTable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\JavaObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\JavaObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\JavaString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\JavaString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\JavaVM.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\JavaVM.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Method.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Method.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Monitor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Monitor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\StackWalker.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\StackWalker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\SysCallsRuntime.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\SysCallsRuntime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Thread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Runtime\System\Thread.h
# End Source File
# End Group
# End Group
# Begin Group "Utilities"

# PROP Default_Filter ""
# Begin Group "General"

# PROP Default_Filter ""
# Begin Group "md No. 1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Utilities\General\md\CatchAssert_md.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Utilities\General\CatchAssert.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\CatchAssert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\CUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\CUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\DebugUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\DebugUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\DoublyLinkedList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Exports.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FastBitMatrix.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FastBitMatrix.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FastBitSet.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FastBitSet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FastHashTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Fifo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FloatUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\FloatUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Fundamentals.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\GraphUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\HashTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\InterestingEvents.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\JavaBytecodes.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\JavaBytecodes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\LogModule.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\LogModule.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\MemoryAccess.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Pool.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Pool.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\StringUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Tree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Tree.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\General\Vector.h
# End Source File
# End Group
# Begin Group "zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\adler32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\compress.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\crc32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\deflate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\deflate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\gzio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\infblock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\infblock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\infcodes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\infcodes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\inffast.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\inflate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\inftrees.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\infutil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\infutil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\trees.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\uncompr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\zutil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\zlib\zutil.h
# End Source File
# End Group
# End Group
# Begin Group "Compiler"

# PROP Default_Filter ""
# Begin Group "FrontEnd"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeGraph.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeGraph.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeTranslator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeTranslator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeVerifier.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeVerifier.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\ErrorHandling.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\ErrorHandling.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\LocalEnv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\TranslationEnv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\TranslationEnv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\VerificationEnv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\FrontEnd\VerificationEnv.h
# End Source File
# End Group
# Begin Group "CodeGenerator"

# PROP Default_Filter ""
# Begin Group "md"

# PROP Default_Filter ""
# Begin Group "x86"

# PROP Default_Filter ""
# Begin Group "X86Disassembler"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsm.h
# End Source File
# End Group
# Begin Source File

SOURCE="..\..\..\Compiler\CodeGenerator\md\x86\x86-win32.nad"

!IF  "$(CFG)" == "electricalfire - Win32 Release"

# Begin Custom Build - Perly Nads
InputPath=..\..\..\Compiler\CodeGenerator\md\x86\x86-win32.nad
InputName=x86-win32

BuildCmds= \
	$(NSTOOLS)\perl5\perl ..\..\..\tools\nad\nad.pl  $(InputPath)\
                                                                           ..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations\
                                                                                                  genfiles\PrimitiveOperations.h genfiles\PrimitiveOperations.cpp\
                                                                                                    genfiles\$(InputName).nad.burg.h | Burg\Release\burg -I >\
                                                                                                genfiles\$(InputName).nad.burg.cpp

"genfiles\$(InputName).nad.burg.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"genfiles\$(InputName).nad.burg.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"genfiles\PrimitiveOperations.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"genfiles\PrimitiveOperations.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

# Begin Custom Build - Perly Nads
InputPath=..\..\..\Compiler\CodeGenerator\md\x86\x86-win32.nad
InputName=x86-win32

BuildCmds= \
	$(MOZ_TOOLS)\perl5\perl ..\..\..\tools\nad\nad.pl  $(InputPath)\
                                                                           ..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations\
                                                                                                  genfiles\PrimitiveOperations.h genfiles\PrimitiveOperations.cpp\
                                                                                                    genfiles\$(InputName).nad.burg.h | Burg\Debug\burg -I >\
                                                                                                genfiles\$(InputName).nad.burg.cpp

"genfiles\$(InputName).nad.burg.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"genfiles\$(InputName).nad.burg.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Float.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86StdCall.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32_Support.cpp
# SUBTRACT CPP /O<none>
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h
# End Source File
# End Group
# Begin Group "generic"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\generic\Generic_Support.cpp
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\md\CpuInfo.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Backend.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Backend.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Burg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\CGScheduler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\CGScheduler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\CodeGenerator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\CodeGenerator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\ExceptionTable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\ExceptionTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\HTMLMethodDump.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\HTMLMethodDump.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\IGVisualizer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\IGVisualizer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Instruction.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Instruction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\InstructionEmitter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\InstructionEmitter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\NativeCodeCache.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\NativeCodeCache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\NativeFormatter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\NativeFormatter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Scheduler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\CodeGenerator\Scheduler.h
# End Source File
# End Group
# Begin Group "RegAlloc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\Coloring.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\Coloring.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\RegisterAllocator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\RegisterAllocator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\Spilling.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\Spilling.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\VirtualRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\RegisterAllocator\VirtualRegister.h
# End Source File
# End Group
# Begin Group "Optimizer"

# PROP Default_Filter ".cpp .h"
# Begin Source File

SOURCE=..\..\..\Compiler\Optimizer\PrimitiveOptimizer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h
# End Source File
# End Group
# Begin Group "PrimitiveGraph"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\Address.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\Address.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\ControlGraph.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\ControlGraph.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\ControlNodes.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\ControlNodes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveGraphVerifier.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations

!IF  "$(CFG)" == "electricalfire - Win32 Release"

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

# Begin Custom Build - PrimitiveOperations
InputPath=..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations
InputName=PrimitiveOperations

BuildCmds= \
	$(MOZ_TOOLS)\perl5\perl -I"..\..\..\Tools\PrimitiveOperations"\
                                            ..\..\..\Tools\PrimitiveOperations\MakePrimOp.pl $(InputPath)\
                                            genfiles\$(InputName).h genfiles\$(InputName).cpp

"genfiles\$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"genfiles\$(InputName).cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\Primitives.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\Primitives.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\SysCalls.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\SysCalls.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\Value.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Compiler\PrimitiveGraph\Value.h
# End Source File
# End Group
# End Group
# Begin Group "Debugger"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Debugger\Debugger.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Debugger\Debugger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Debugger\jvmdi.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\Debugger\jvmdi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Debugger\Win32BreakPoint.cpp
# End Source File
# End Group
# Begin Group "Exports"

# PROP Default_Filter ""
# Begin Group "md No. 3"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Exports\md\jni_md.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Exports\jni.h
# End Source File
# End Group
# Begin Group "Includes"

# PROP Default_Filter ""
# Begin Group "md No. 4"

# PROP Default_Filter ""
# Begin Group "x86 No. 2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Includes\md\x86\x86Asm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Includes\md\x86\x86LinuxAsm.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Includes\md\Asm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Includes\md\MdInclude.h
# End Source File
# End Group
# End Group
# End Target
# End Project
