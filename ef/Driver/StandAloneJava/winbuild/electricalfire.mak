# Microsoft Developer Studio Generated NMAKE File, Based on electricalfire.dsp
!IF "$(CFG)" == ""
CFG=electricalfire - Win32 Debug
!MESSAGE No configuration specified. Defaulting to electricalfire - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "electricalfire - Win32 Release" && "$(CFG)" !=\
 "electricalfire - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "electricalfire - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "genfiles\$(InputName).nad.burg.h" "genfiles\$(InputName).nad.burg.cpp"\
 "$(OUTDIR)\electricalfire.dll" "$(OUTDIR)\electricalfire.bsc"

!ELSE 

ALL : "genfiles\PrimitiveOperations.h" "genfiles\PrimitiveOperations.cpp"\
 "genfiles\$(InputName).nad.burg.h" "genfiles\$(InputName).nad.burg.cpp"\
 "$(OUTDIR)\electricalfire.dll" "$(OUTDIR)\electricalfire.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"Burg - Win32 ReleaseCLEAN" "DebuggerChannel - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\Address.obj"
	-@erase "$(INTDIR)\Address.sbr"
	-@erase "$(INTDIR)\adler32.obj"
	-@erase "$(INTDIR)\adler32.sbr"
	-@erase "$(INTDIR)\AttributeHandlers.obj"
	-@erase "$(INTDIR)\AttributeHandlers.sbr"
	-@erase "$(INTDIR)\Backend.obj"
	-@erase "$(INTDIR)\Backend.sbr"
	-@erase "$(INTDIR)\BufferedFileReader.obj"
	-@erase "$(INTDIR)\BufferedFileReader.sbr"
	-@erase "$(INTDIR)\BytecodeGraph.obj"
	-@erase "$(INTDIR)\BytecodeGraph.sbr"
	-@erase "$(INTDIR)\BytecodeTranslator.obj"
	-@erase "$(INTDIR)\BytecodeTranslator.sbr"
	-@erase "$(INTDIR)\BytecodeVerifier.obj"
	-@erase "$(INTDIR)\BytecodeVerifier.sbr"
	-@erase "$(INTDIR)\CatchAssert.obj"
	-@erase "$(INTDIR)\CatchAssert.sbr"
	-@erase "$(INTDIR)\CGScheduler.obj"
	-@erase "$(INTDIR)\CGScheduler.sbr"
	-@erase "$(INTDIR)\ClassCentral.obj"
	-@erase "$(INTDIR)\ClassCentral.sbr"
	-@erase "$(INTDIR)\ClassFileSummary.obj"
	-@erase "$(INTDIR)\ClassFileSummary.sbr"
	-@erase "$(INTDIR)\ClassReader.obj"
	-@erase "$(INTDIR)\ClassReader.sbr"
	-@erase "$(INTDIR)\ClassWorld.obj"
	-@erase "$(INTDIR)\ClassWorld.sbr"
	-@erase "$(INTDIR)\CodeGenerator.obj"
	-@erase "$(INTDIR)\CodeGenerator.sbr"
	-@erase "$(INTDIR)\Coloring.obj"
	-@erase "$(INTDIR)\Coloring.sbr"
	-@erase "$(INTDIR)\compress.obj"
	-@erase "$(INTDIR)\compress.sbr"
	-@erase "$(INTDIR)\ControlGraph.obj"
	-@erase "$(INTDIR)\ControlGraph.sbr"
	-@erase "$(INTDIR)\ControlNodes.obj"
	-@erase "$(INTDIR)\ControlNodes.sbr"
	-@erase "$(INTDIR)\crc32.obj"
	-@erase "$(INTDIR)\crc32.sbr"
	-@erase "$(INTDIR)\CUtils.obj"
	-@erase "$(INTDIR)\CUtils.sbr"
	-@erase "$(INTDIR)\Debugger.obj"
	-@erase "$(INTDIR)\Debugger.sbr"
	-@erase "$(INTDIR)\DebugUtils.obj"
	-@erase "$(INTDIR)\DebugUtils.sbr"
	-@erase "$(INTDIR)\deflate.obj"
	-@erase "$(INTDIR)\deflate.sbr"
	-@erase "$(INTDIR)\DiskFileReader.obj"
	-@erase "$(INTDIR)\DiskFileReader.sbr"
	-@erase "$(INTDIR)\ErrorHandling.obj"
	-@erase "$(INTDIR)\ErrorHandling.sbr"
	-@erase "$(INTDIR)\ExceptionTable.obj"
	-@erase "$(INTDIR)\ExceptionTable.sbr"
	-@erase "$(INTDIR)\FastBitMatrix.obj"
	-@erase "$(INTDIR)\FastBitMatrix.sbr"
	-@erase "$(INTDIR)\FastBitSet.obj"
	-@erase "$(INTDIR)\FastBitSet.sbr"
	-@erase "$(INTDIR)\FieldOrMethod.obj"
	-@erase "$(INTDIR)\FieldOrMethod.sbr"
	-@erase "$(INTDIR)\FileReader.obj"
	-@erase "$(INTDIR)\FileReader.sbr"
	-@erase "$(INTDIR)\FloatUtils.obj"
	-@erase "$(INTDIR)\FloatUtils.sbr"
	-@erase "$(INTDIR)\gzio.obj"
	-@erase "$(INTDIR)\gzio.sbr"
	-@erase "$(INTDIR)\HTMLMethodDump.obj"
	-@erase "$(INTDIR)\HTMLMethodDump.sbr"
	-@erase "$(INTDIR)\IGVisualizer.obj"
	-@erase "$(INTDIR)\IGVisualizer.sbr"
	-@erase "$(INTDIR)\infblock.obj"
	-@erase "$(INTDIR)\infblock.sbr"
	-@erase "$(INTDIR)\infcodes.obj"
	-@erase "$(INTDIR)\infcodes.sbr"
	-@erase "$(INTDIR)\inffast.obj"
	-@erase "$(INTDIR)\inffast.sbr"
	-@erase "$(INTDIR)\inflate.obj"
	-@erase "$(INTDIR)\inflate.sbr"
	-@erase "$(INTDIR)\inftrees.obj"
	-@erase "$(INTDIR)\inftrees.sbr"
	-@erase "$(INTDIR)\infutil.obj"
	-@erase "$(INTDIR)\infutil.sbr"
	-@erase "$(INTDIR)\Instruction.obj"
	-@erase "$(INTDIR)\Instruction.sbr"
	-@erase "$(INTDIR)\InstructionEmitter.obj"
	-@erase "$(INTDIR)\InstructionEmitter.sbr"
	-@erase "$(INTDIR)\InterestingEvents.obj"
	-@erase "$(INTDIR)\InterestingEvents.sbr"
	-@erase "$(INTDIR)\InterfaceDispatchTable.obj"
	-@erase "$(INTDIR)\InterfaceDispatchTable.sbr"
	-@erase "$(INTDIR)\JavaBytecodes.obj"
	-@erase "$(INTDIR)\JavaBytecodes.sbr"
	-@erase "$(INTDIR)\JavaObject.obj"
	-@erase "$(INTDIR)\JavaObject.sbr"
	-@erase "$(INTDIR)\JavaString.obj"
	-@erase "$(INTDIR)\JavaString.sbr"
	-@erase "$(INTDIR)\JavaVM.obj"
	-@erase "$(INTDIR)\JavaVM.sbr"
	-@erase "$(INTDIR)\Jni.obj"
	-@erase "$(INTDIR)\Jni.sbr"
	-@erase "$(INTDIR)\JNIManglers.obj"
	-@erase "$(INTDIR)\JNIManglers.sbr"
	-@erase "$(INTDIR)\jvmdi.obj"
	-@erase "$(INTDIR)\jvmdi.sbr"
	-@erase "$(INTDIR)\LogModule.obj"
	-@erase "$(INTDIR)\LogModule.sbr"
	-@erase "$(INTDIR)\Method.obj"
	-@erase "$(INTDIR)\Method.sbr"
	-@erase "$(INTDIR)\Monitor.obj"
	-@erase "$(INTDIR)\Monitor.sbr"
	-@erase "$(INTDIR)\NameMangler.obj"
	-@erase "$(INTDIR)\NameMangler.sbr"
	-@erase "$(INTDIR)\NativeCodeCache.obj"
	-@erase "$(INTDIR)\NativeCodeCache.sbr"
	-@erase "$(INTDIR)\NativeFormatter.obj"
	-@erase "$(INTDIR)\NativeFormatter.sbr"
	-@erase "$(INTDIR)\NativeMethodDispatcher.obj"
	-@erase "$(INTDIR)\NativeMethodDispatcher.sbr"
	-@erase "$(INTDIR)\NetscapeManglers.obj"
	-@erase "$(INTDIR)\NetscapeManglers.sbr"
	-@erase "$(INTDIR)\PathComponent.obj"
	-@erase "$(INTDIR)\PathComponent.sbr"
	-@erase "$(INTDIR)\Pool.obj"
	-@erase "$(INTDIR)\Pool.sbr"
	-@erase "$(INTDIR)\PrimitiveBuilders.obj"
	-@erase "$(INTDIR)\PrimitiveBuilders.sbr"
	-@erase "$(INTDIR)\PrimitiveGraphVerifier.obj"
	-@erase "$(INTDIR)\PrimitiveGraphVerifier.sbr"
	-@erase "$(INTDIR)\PrimitiveOptimizer.obj"
	-@erase "$(INTDIR)\PrimitiveOptimizer.sbr"
	-@erase "$(INTDIR)\Primitives.obj"
	-@erase "$(INTDIR)\Primitives.sbr"
	-@erase "$(INTDIR)\RegisterAllocator.obj"
	-@erase "$(INTDIR)\RegisterAllocator.sbr"
	-@erase "$(INTDIR)\Scheduler.obj"
	-@erase "$(INTDIR)\Scheduler.sbr"
	-@erase "$(INTDIR)\Spilling.obj"
	-@erase "$(INTDIR)\Spilling.sbr"
	-@erase "$(INTDIR)\StackWalker.obj"
	-@erase "$(INTDIR)\StackWalker.sbr"
	-@erase "$(INTDIR)\SysCalls.obj"
	-@erase "$(INTDIR)\SysCalls.sbr"
	-@erase "$(INTDIR)\SysCallsRuntime.obj"
	-@erase "$(INTDIR)\SysCallsRuntime.sbr"
	-@erase "$(INTDIR)\Thread.obj"
	-@erase "$(INTDIR)\Thread.sbr"
	-@erase "$(INTDIR)\TranslationEnv.obj"
	-@erase "$(INTDIR)\TranslationEnv.sbr"
	-@erase "$(INTDIR)\Tree.obj"
	-@erase "$(INTDIR)\Tree.sbr"
	-@erase "$(INTDIR)\trees.obj"
	-@erase "$(INTDIR)\trees.sbr"
	-@erase "$(INTDIR)\uncompr.obj"
	-@erase "$(INTDIR)\uncompr.sbr"
	-@erase "$(INTDIR)\Value.obj"
	-@erase "$(INTDIR)\Value.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\VerificationEnv.obj"
	-@erase "$(INTDIR)\VerificationEnv.sbr"
	-@erase "$(INTDIR)\VirtualRegister.obj"
	-@erase "$(INTDIR)\VirtualRegister.sbr"
	-@erase "$(INTDIR)\Win32BreakPoint.obj"
	-@erase "$(INTDIR)\Win32BreakPoint.sbr"
	-@erase "$(INTDIR)\Win32ExceptionHandler.obj"
	-@erase "$(INTDIR)\Win32ExceptionHandler.sbr"
	-@erase "$(INTDIR)\x86-win32.nad.burg.obj"
	-@erase "$(INTDIR)\x86-win32.nad.burg.sbr"
	-@erase "$(INTDIR)\x86ArgumentList.obj"
	-@erase "$(INTDIR)\x86ArgumentList.sbr"
	-@erase "$(INTDIR)\x86ExceptionHandler.obj"
	-@erase "$(INTDIR)\x86ExceptionHandler.sbr"
	-@erase "$(INTDIR)\x86Float.obj"
	-@erase "$(INTDIR)\x86Float.sbr"
	-@erase "$(INTDIR)\x86Formatter.obj"
	-@erase "$(INTDIR)\x86Formatter.sbr"
	-@erase "$(INTDIR)\x86NativeMethodStubs.obj"
	-@erase "$(INTDIR)\x86NativeMethodStubs.sbr"
	-@erase "$(INTDIR)\x86Opcode.obj"
	-@erase "$(INTDIR)\x86Opcode.sbr"
	-@erase "$(INTDIR)\x86StdCall.obj"
	-@erase "$(INTDIR)\x86StdCall.sbr"
	-@erase "$(INTDIR)\x86SysCallsRuntime.obj"
	-@erase "$(INTDIR)\x86SysCallsRuntime.sbr"
	-@erase "$(INTDIR)\x86Win32_Support.obj"
	-@erase "$(INTDIR)\x86Win32_Support.sbr"
	-@erase "$(INTDIR)\x86Win32Emitter.obj"
	-@erase "$(INTDIR)\x86Win32Emitter.sbr"
	-@erase "$(INTDIR)\x86Win32Instruction.obj"
	-@erase "$(INTDIR)\x86Win32Instruction.sbr"
	-@erase "$(INTDIR)\x86Win32InvokeNative.obj"
	-@erase "$(INTDIR)\x86Win32InvokeNative.sbr"
	-@erase "$(INTDIR)\x86Win32Thread.obj"
	-@erase "$(INTDIR)\x86Win32Thread.sbr"
	-@erase "$(INTDIR)\XDisAsm.obj"
	-@erase "$(INTDIR)\XDisAsm.sbr"
	-@erase "$(INTDIR)\XDisAsmTable.obj"
	-@erase "$(INTDIR)\XDisAsmTable.sbr"
	-@erase "$(INTDIR)\ZipArchive.obj"
	-@erase "$(INTDIR)\ZipArchive.sbr"
	-@erase "$(INTDIR)\ZipFileReader.obj"
	-@erase "$(INTDIR)\ZipFileReader.sbr"
	-@erase "$(INTDIR)\zutil.obj"
	-@erase "$(INTDIR)\zutil.sbr"
	-@erase "$(OUTDIR)\electricalfire.bsc"
	-@erase "$(OUTDIR)\electricalfire.dll"
	-@erase "$(OUTDIR)\electricalfire.exp"
	-@erase "$(OUTDIR)\electricalfire.lib"
	-@erase "genfiles\$(InputName).nad.burg.cpp"
	-@erase "genfiles\$(InputName).nad.burg.h"
	-@erase "genfiles\PrimitiveOperations.cpp"
	-@erase "genfiles\PrimitiveOperations.h"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\..\..\Exports" /I "..\..\..\Exports\md"\
 /I "..\..\..\Utilities\General" /I "..\..\..\Utilities\General\md" /I\
 "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I\
 "..\..\..\Runtime\System\md\x86" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include"\
 /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I\
 "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I\
 "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I\
 "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86" /I\
 "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I\
 "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I\
 "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I\
 "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "NDEBUG"\
 /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "XP_PC" /D "DLL_BUILD" /D "WIN32" /D\
 "_CONSOLE" /D "_MBCS" /D "GENERATE_NATIVE_STUBS" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\electricalfire.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\Release/
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\electricalfire.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\Address.sbr" \
	"$(INTDIR)\adler32.sbr" \
	"$(INTDIR)\AttributeHandlers.sbr" \
	"$(INTDIR)\Backend.sbr" \
	"$(INTDIR)\BufferedFileReader.sbr" \
	"$(INTDIR)\BytecodeGraph.sbr" \
	"$(INTDIR)\BytecodeTranslator.sbr" \
	"$(INTDIR)\BytecodeVerifier.sbr" \
	"$(INTDIR)\CatchAssert.sbr" \
	"$(INTDIR)\CGScheduler.sbr" \
	"$(INTDIR)\ClassCentral.sbr" \
	"$(INTDIR)\ClassFileSummary.sbr" \
	"$(INTDIR)\ClassReader.sbr" \
	"$(INTDIR)\ClassWorld.sbr" \
	"$(INTDIR)\CodeGenerator.sbr" \
	"$(INTDIR)\Coloring.sbr" \
	"$(INTDIR)\compress.sbr" \
	"$(INTDIR)\ControlGraph.sbr" \
	"$(INTDIR)\ControlNodes.sbr" \
	"$(INTDIR)\crc32.sbr" \
	"$(INTDIR)\CUtils.sbr" \
	"$(INTDIR)\Debugger.sbr" \
	"$(INTDIR)\DebugUtils.sbr" \
	"$(INTDIR)\deflate.sbr" \
	"$(INTDIR)\DiskFileReader.sbr" \
	"$(INTDIR)\ErrorHandling.sbr" \
	"$(INTDIR)\ExceptionTable.sbr" \
	"$(INTDIR)\FastBitMatrix.sbr" \
	"$(INTDIR)\FastBitSet.sbr" \
	"$(INTDIR)\FieldOrMethod.sbr" \
	"$(INTDIR)\FileReader.sbr" \
	"$(INTDIR)\FloatUtils.sbr" \
	"$(INTDIR)\gzio.sbr" \
	"$(INTDIR)\HTMLMethodDump.sbr" \
	"$(INTDIR)\IGVisualizer.sbr" \
	"$(INTDIR)\infblock.sbr" \
	"$(INTDIR)\infcodes.sbr" \
	"$(INTDIR)\inffast.sbr" \
	"$(INTDIR)\inflate.sbr" \
	"$(INTDIR)\inftrees.sbr" \
	"$(INTDIR)\infutil.sbr" \
	"$(INTDIR)\Instruction.sbr" \
	"$(INTDIR)\InstructionEmitter.sbr" \
	"$(INTDIR)\InterestingEvents.sbr" \
	"$(INTDIR)\InterfaceDispatchTable.sbr" \
	"$(INTDIR)\JavaBytecodes.sbr" \
	"$(INTDIR)\JavaObject.sbr" \
	"$(INTDIR)\JavaString.sbr" \
	"$(INTDIR)\JavaVM.sbr" \
	"$(INTDIR)\Jni.sbr" \
	"$(INTDIR)\JNIManglers.sbr" \
	"$(INTDIR)\jvmdi.sbr" \
	"$(INTDIR)\LogModule.sbr" \
	"$(INTDIR)\Method.sbr" \
	"$(INTDIR)\Monitor.sbr" \
	"$(INTDIR)\NameMangler.sbr" \
	"$(INTDIR)\NativeCodeCache.sbr" \
	"$(INTDIR)\NativeFormatter.sbr" \
	"$(INTDIR)\NativeMethodDispatcher.sbr" \
	"$(INTDIR)\NetscapeManglers.sbr" \
	"$(INTDIR)\PathComponent.sbr" \
	"$(INTDIR)\Pool.sbr" \
	"$(INTDIR)\PrimitiveBuilders.sbr" \
	"$(INTDIR)\PrimitiveGraphVerifier.sbr" \
	"$(INTDIR)\PrimitiveOptimizer.sbr" \
	"$(INTDIR)\Primitives.sbr" \
	"$(INTDIR)\RegisterAllocator.sbr" \
	"$(INTDIR)\Scheduler.sbr" \
	"$(INTDIR)\Spilling.sbr" \
	"$(INTDIR)\StackWalker.sbr" \
	"$(INTDIR)\SysCalls.sbr" \
	"$(INTDIR)\SysCallsRuntime.sbr" \
	"$(INTDIR)\Thread.sbr" \
	"$(INTDIR)\TranslationEnv.sbr" \
	"$(INTDIR)\Tree.sbr" \
	"$(INTDIR)\trees.sbr" \
	"$(INTDIR)\uncompr.sbr" \
	"$(INTDIR)\Value.sbr" \
	"$(INTDIR)\VerificationEnv.sbr" \
	"$(INTDIR)\VirtualRegister.sbr" \
	"$(INTDIR)\Win32BreakPoint.sbr" \
	"$(INTDIR)\Win32ExceptionHandler.sbr" \
	"$(INTDIR)\x86-win32.nad.burg.sbr" \
	"$(INTDIR)\x86ArgumentList.sbr" \
	"$(INTDIR)\x86ExceptionHandler.sbr" \
	"$(INTDIR)\x86Float.sbr" \
	"$(INTDIR)\x86Formatter.sbr" \
	"$(INTDIR)\x86NativeMethodStubs.sbr" \
	"$(INTDIR)\x86Opcode.sbr" \
	"$(INTDIR)\x86StdCall.sbr" \
	"$(INTDIR)\x86SysCallsRuntime.sbr" \
	"$(INTDIR)\x86Win32_Support.sbr" \
	"$(INTDIR)\x86Win32Emitter.sbr" \
	"$(INTDIR)\x86Win32Instruction.sbr" \
	"$(INTDIR)\x86Win32InvokeNative.sbr" \
	"$(INTDIR)\x86Win32Thread.sbr" \
	"$(INTDIR)\XDisAsm.sbr" \
	"$(INTDIR)\XDisAsmTable.sbr" \
	"$(INTDIR)\ZipArchive.sbr" \
	"$(INTDIR)\ZipFileReader.sbr" \
	"$(INTDIR)\zutil.sbr"

"$(OUTDIR)\electricalfire.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=electric\DebuggerChannel.lib\
 ..\..\..\..\dist\WINNT4.0_OPT.OBJ\lib\libnspr20.lib\
 ..\..\..\..\dist\WINNT4.0_OPT.OBJ\lib\libplc20_s.lib kernel32.lib user32.lib\
 gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib\
 oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo\
 /subsystem:console /dll /incremental:no /pdb:"$(OUTDIR)\electricalfire.pdb"\
 /machine:I386 /nodefaultlib:"MSVCRT" /out:"$(OUTDIR)\electricalfire.dll"\
 /implib:"$(OUTDIR)\electricalfire.lib" 
LINK32_OBJS= \
	"$(INTDIR)\Address.obj" \
	"$(INTDIR)\adler32.obj" \
	"$(INTDIR)\AttributeHandlers.obj" \
	"$(INTDIR)\Backend.obj" \
	"$(INTDIR)\BufferedFileReader.obj" \
	"$(INTDIR)\BytecodeGraph.obj" \
	"$(INTDIR)\BytecodeTranslator.obj" \
	"$(INTDIR)\BytecodeVerifier.obj" \
	"$(INTDIR)\CatchAssert.obj" \
	"$(INTDIR)\CGScheduler.obj" \
	"$(INTDIR)\ClassCentral.obj" \
	"$(INTDIR)\ClassFileSummary.obj" \
	"$(INTDIR)\ClassReader.obj" \
	"$(INTDIR)\ClassWorld.obj" \
	"$(INTDIR)\CodeGenerator.obj" \
	"$(INTDIR)\Coloring.obj" \
	"$(INTDIR)\compress.obj" \
	"$(INTDIR)\ControlGraph.obj" \
	"$(INTDIR)\ControlNodes.obj" \
	"$(INTDIR)\crc32.obj" \
	"$(INTDIR)\CUtils.obj" \
	"$(INTDIR)\Debugger.obj" \
	"$(INTDIR)\DebugUtils.obj" \
	"$(INTDIR)\deflate.obj" \
	"$(INTDIR)\DiskFileReader.obj" \
	"$(INTDIR)\ErrorHandling.obj" \
	"$(INTDIR)\ExceptionTable.obj" \
	"$(INTDIR)\FastBitMatrix.obj" \
	"$(INTDIR)\FastBitSet.obj" \
	"$(INTDIR)\FieldOrMethod.obj" \
	"$(INTDIR)\FileReader.obj" \
	"$(INTDIR)\FloatUtils.obj" \
	"$(INTDIR)\gzio.obj" \
	"$(INTDIR)\HTMLMethodDump.obj" \
	"$(INTDIR)\IGVisualizer.obj" \
	"$(INTDIR)\infblock.obj" \
	"$(INTDIR)\infcodes.obj" \
	"$(INTDIR)\inffast.obj" \
	"$(INTDIR)\inflate.obj" \
	"$(INTDIR)\inftrees.obj" \
	"$(INTDIR)\infutil.obj" \
	"$(INTDIR)\Instruction.obj" \
	"$(INTDIR)\InstructionEmitter.obj" \
	"$(INTDIR)\InterestingEvents.obj" \
	"$(INTDIR)\InterfaceDispatchTable.obj" \
	"$(INTDIR)\JavaBytecodes.obj" \
	"$(INTDIR)\JavaObject.obj" \
	"$(INTDIR)\JavaString.obj" \
	"$(INTDIR)\JavaVM.obj" \
	"$(INTDIR)\Jni.obj" \
	"$(INTDIR)\JNIManglers.obj" \
	"$(INTDIR)\jvmdi.obj" \
	"$(INTDIR)\LogModule.obj" \
	"$(INTDIR)\Method.obj" \
	"$(INTDIR)\Monitor.obj" \
	"$(INTDIR)\NameMangler.obj" \
	"$(INTDIR)\NativeCodeCache.obj" \
	"$(INTDIR)\NativeFormatter.obj" \
	"$(INTDIR)\NativeMethodDispatcher.obj" \
	"$(INTDIR)\NetscapeManglers.obj" \
	"$(INTDIR)\PathComponent.obj" \
	"$(INTDIR)\Pool.obj" \
	"$(INTDIR)\PrimitiveBuilders.obj" \
	"$(INTDIR)\PrimitiveGraphVerifier.obj" \
	"$(INTDIR)\PrimitiveOptimizer.obj" \
	"$(INTDIR)\Primitives.obj" \
	"$(INTDIR)\RegisterAllocator.obj" \
	"$(INTDIR)\Scheduler.obj" \
	"$(INTDIR)\Spilling.obj" \
	"$(INTDIR)\StackWalker.obj" \
	"$(INTDIR)\SysCalls.obj" \
	"$(INTDIR)\SysCallsRuntime.obj" \
	"$(INTDIR)\Thread.obj" \
	"$(INTDIR)\TranslationEnv.obj" \
	"$(INTDIR)\Tree.obj" \
	"$(INTDIR)\trees.obj" \
	"$(INTDIR)\uncompr.obj" \
	"$(INTDIR)\Value.obj" \
	"$(INTDIR)\VerificationEnv.obj" \
	"$(INTDIR)\VirtualRegister.obj" \
	"$(INTDIR)\Win32BreakPoint.obj" \
	"$(INTDIR)\Win32ExceptionHandler.obj" \
	"$(INTDIR)\x86-win32.nad.burg.obj" \
	"$(INTDIR)\x86ArgumentList.obj" \
	"$(INTDIR)\x86ExceptionHandler.obj" \
	"$(INTDIR)\x86Float.obj" \
	"$(INTDIR)\x86Formatter.obj" \
	"$(INTDIR)\x86NativeMethodStubs.obj" \
	"$(INTDIR)\x86Opcode.obj" \
	"$(INTDIR)\x86StdCall.obj" \
	"$(INTDIR)\x86SysCallsRuntime.obj" \
	"$(INTDIR)\x86Win32_Support.obj" \
	"$(INTDIR)\x86Win32Emitter.obj" \
	"$(INTDIR)\x86Win32Instruction.obj" \
	"$(INTDIR)\x86Win32InvokeNative.obj" \
	"$(INTDIR)\x86Win32Thread.obj" \
	"$(INTDIR)\XDisAsm.obj" \
	"$(INTDIR)\XDisAsmTable.obj" \
	"$(INTDIR)\ZipArchive.obj" \
	"$(INTDIR)\ZipFileReader.obj" \
	"$(INTDIR)\zutil.obj" \
	"$(OUTDIR)\DebuggerChannel.lib"

"$(OUTDIR)\electricalfire.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

OUTDIR=.\electric
INTDIR=.\electric
# Begin Custom Macros
OutDir=.\electric
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "genfiles\$(InputName).h" "genfiles\$(InputName).cpp"\
 "$(OUTDIR)\electricalfire.dll" "$(OUTDIR)\electricalfire.bsc"

!ELSE 

ALL : "genfiles\$(InputName).h" "genfiles\$(InputName).cpp"\
 "$(OUTDIR)\electricalfire.dll" "$(OUTDIR)\electricalfire.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"Burg - Win32 DebugCLEAN" "DebuggerChannel - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\Address.obj"
	-@erase "$(INTDIR)\Address.sbr"
	-@erase "$(INTDIR)\adler32.obj"
	-@erase "$(INTDIR)\adler32.sbr"
	-@erase "$(INTDIR)\AttributeHandlers.obj"
	-@erase "$(INTDIR)\AttributeHandlers.sbr"
	-@erase "$(INTDIR)\Backend.obj"
	-@erase "$(INTDIR)\Backend.sbr"
	-@erase "$(INTDIR)\BufferedFileReader.obj"
	-@erase "$(INTDIR)\BufferedFileReader.sbr"
	-@erase "$(INTDIR)\BytecodeGraph.obj"
	-@erase "$(INTDIR)\BytecodeGraph.sbr"
	-@erase "$(INTDIR)\BytecodeTranslator.obj"
	-@erase "$(INTDIR)\BytecodeTranslator.sbr"
	-@erase "$(INTDIR)\BytecodeVerifier.obj"
	-@erase "$(INTDIR)\BytecodeVerifier.sbr"
	-@erase "$(INTDIR)\CatchAssert.obj"
	-@erase "$(INTDIR)\CatchAssert.sbr"
	-@erase "$(INTDIR)\CGScheduler.obj"
	-@erase "$(INTDIR)\CGScheduler.sbr"
	-@erase "$(INTDIR)\ClassCentral.obj"
	-@erase "$(INTDIR)\ClassCentral.sbr"
	-@erase "$(INTDIR)\ClassFileSummary.obj"
	-@erase "$(INTDIR)\ClassFileSummary.sbr"
	-@erase "$(INTDIR)\ClassReader.obj"
	-@erase "$(INTDIR)\ClassReader.sbr"
	-@erase "$(INTDIR)\ClassWorld.obj"
	-@erase "$(INTDIR)\ClassWorld.sbr"
	-@erase "$(INTDIR)\CodeGenerator.obj"
	-@erase "$(INTDIR)\CodeGenerator.sbr"
	-@erase "$(INTDIR)\Coloring.obj"
	-@erase "$(INTDIR)\Coloring.sbr"
	-@erase "$(INTDIR)\compress.obj"
	-@erase "$(INTDIR)\compress.sbr"
	-@erase "$(INTDIR)\ControlGraph.obj"
	-@erase "$(INTDIR)\ControlGraph.sbr"
	-@erase "$(INTDIR)\ControlNodes.obj"
	-@erase "$(INTDIR)\ControlNodes.sbr"
	-@erase "$(INTDIR)\crc32.obj"
	-@erase "$(INTDIR)\crc32.sbr"
	-@erase "$(INTDIR)\CUtils.obj"
	-@erase "$(INTDIR)\CUtils.sbr"
	-@erase "$(INTDIR)\Debugger.obj"
	-@erase "$(INTDIR)\Debugger.sbr"
	-@erase "$(INTDIR)\DebugUtils.obj"
	-@erase "$(INTDIR)\DebugUtils.sbr"
	-@erase "$(INTDIR)\deflate.obj"
	-@erase "$(INTDIR)\deflate.sbr"
	-@erase "$(INTDIR)\DiskFileReader.obj"
	-@erase "$(INTDIR)\DiskFileReader.sbr"
	-@erase "$(INTDIR)\ErrorHandling.obj"
	-@erase "$(INTDIR)\ErrorHandling.sbr"
	-@erase "$(INTDIR)\ExceptionTable.obj"
	-@erase "$(INTDIR)\ExceptionTable.sbr"
	-@erase "$(INTDIR)\FastBitMatrix.obj"
	-@erase "$(INTDIR)\FastBitMatrix.sbr"
	-@erase "$(INTDIR)\FastBitSet.obj"
	-@erase "$(INTDIR)\FastBitSet.sbr"
	-@erase "$(INTDIR)\FieldOrMethod.obj"
	-@erase "$(INTDIR)\FieldOrMethod.sbr"
	-@erase "$(INTDIR)\FileReader.obj"
	-@erase "$(INTDIR)\FileReader.sbr"
	-@erase "$(INTDIR)\FloatUtils.obj"
	-@erase "$(INTDIR)\FloatUtils.sbr"
	-@erase "$(INTDIR)\gzio.obj"
	-@erase "$(INTDIR)\gzio.sbr"
	-@erase "$(INTDIR)\HTMLMethodDump.obj"
	-@erase "$(INTDIR)\HTMLMethodDump.sbr"
	-@erase "$(INTDIR)\IGVisualizer.obj"
	-@erase "$(INTDIR)\IGVisualizer.sbr"
	-@erase "$(INTDIR)\infblock.obj"
	-@erase "$(INTDIR)\infblock.sbr"
	-@erase "$(INTDIR)\infcodes.obj"
	-@erase "$(INTDIR)\infcodes.sbr"
	-@erase "$(INTDIR)\inffast.obj"
	-@erase "$(INTDIR)\inffast.sbr"
	-@erase "$(INTDIR)\inflate.obj"
	-@erase "$(INTDIR)\inflate.sbr"
	-@erase "$(INTDIR)\inftrees.obj"
	-@erase "$(INTDIR)\inftrees.sbr"
	-@erase "$(INTDIR)\infutil.obj"
	-@erase "$(INTDIR)\infutil.sbr"
	-@erase "$(INTDIR)\Instruction.obj"
	-@erase "$(INTDIR)\Instruction.sbr"
	-@erase "$(INTDIR)\InstructionEmitter.obj"
	-@erase "$(INTDIR)\InstructionEmitter.sbr"
	-@erase "$(INTDIR)\InterestingEvents.obj"
	-@erase "$(INTDIR)\InterestingEvents.sbr"
	-@erase "$(INTDIR)\InterfaceDispatchTable.obj"
	-@erase "$(INTDIR)\InterfaceDispatchTable.sbr"
	-@erase "$(INTDIR)\JavaBytecodes.obj"
	-@erase "$(INTDIR)\JavaBytecodes.sbr"
	-@erase "$(INTDIR)\JavaObject.obj"
	-@erase "$(INTDIR)\JavaObject.sbr"
	-@erase "$(INTDIR)\JavaString.obj"
	-@erase "$(INTDIR)\JavaString.sbr"
	-@erase "$(INTDIR)\JavaVM.obj"
	-@erase "$(INTDIR)\JavaVM.sbr"
	-@erase "$(INTDIR)\Jni.obj"
	-@erase "$(INTDIR)\Jni.sbr"
	-@erase "$(INTDIR)\JNIManglers.obj"
	-@erase "$(INTDIR)\JNIManglers.sbr"
	-@erase "$(INTDIR)\jvmdi.obj"
	-@erase "$(INTDIR)\jvmdi.sbr"
	-@erase "$(INTDIR)\LogModule.obj"
	-@erase "$(INTDIR)\LogModule.sbr"
	-@erase "$(INTDIR)\Method.obj"
	-@erase "$(INTDIR)\Method.sbr"
	-@erase "$(INTDIR)\Monitor.obj"
	-@erase "$(INTDIR)\Monitor.sbr"
	-@erase "$(INTDIR)\NameMangler.obj"
	-@erase "$(INTDIR)\NameMangler.sbr"
	-@erase "$(INTDIR)\NativeCodeCache.obj"
	-@erase "$(INTDIR)\NativeCodeCache.sbr"
	-@erase "$(INTDIR)\NativeFormatter.obj"
	-@erase "$(INTDIR)\NativeFormatter.sbr"
	-@erase "$(INTDIR)\NativeMethodDispatcher.obj"
	-@erase "$(INTDIR)\NativeMethodDispatcher.sbr"
	-@erase "$(INTDIR)\NetscapeManglers.obj"
	-@erase "$(INTDIR)\NetscapeManglers.sbr"
	-@erase "$(INTDIR)\PathComponent.obj"
	-@erase "$(INTDIR)\PathComponent.sbr"
	-@erase "$(INTDIR)\Pool.obj"
	-@erase "$(INTDIR)\Pool.sbr"
	-@erase "$(INTDIR)\PrimitiveBuilders.obj"
	-@erase "$(INTDIR)\PrimitiveBuilders.sbr"
	-@erase "$(INTDIR)\PrimitiveGraphVerifier.obj"
	-@erase "$(INTDIR)\PrimitiveGraphVerifier.sbr"
	-@erase "$(INTDIR)\PrimitiveOptimizer.obj"
	-@erase "$(INTDIR)\PrimitiveOptimizer.sbr"
	-@erase "$(INTDIR)\Primitives.obj"
	-@erase "$(INTDIR)\Primitives.sbr"
	-@erase "$(INTDIR)\RegisterAllocator.obj"
	-@erase "$(INTDIR)\RegisterAllocator.sbr"
	-@erase "$(INTDIR)\Scheduler.obj"
	-@erase "$(INTDIR)\Scheduler.sbr"
	-@erase "$(INTDIR)\Spilling.obj"
	-@erase "$(INTDIR)\Spilling.sbr"
	-@erase "$(INTDIR)\StackWalker.obj"
	-@erase "$(INTDIR)\StackWalker.sbr"
	-@erase "$(INTDIR)\SysCalls.obj"
	-@erase "$(INTDIR)\SysCalls.sbr"
	-@erase "$(INTDIR)\SysCallsRuntime.obj"
	-@erase "$(INTDIR)\SysCallsRuntime.sbr"
	-@erase "$(INTDIR)\Thread.obj"
	-@erase "$(INTDIR)\Thread.sbr"
	-@erase "$(INTDIR)\TranslationEnv.obj"
	-@erase "$(INTDIR)\TranslationEnv.sbr"
	-@erase "$(INTDIR)\Tree.obj"
	-@erase "$(INTDIR)\Tree.sbr"
	-@erase "$(INTDIR)\trees.obj"
	-@erase "$(INTDIR)\trees.sbr"
	-@erase "$(INTDIR)\uncompr.obj"
	-@erase "$(INTDIR)\uncompr.sbr"
	-@erase "$(INTDIR)\Value.obj"
	-@erase "$(INTDIR)\Value.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\VerificationEnv.obj"
	-@erase "$(INTDIR)\VerificationEnv.sbr"
	-@erase "$(INTDIR)\VirtualRegister.obj"
	-@erase "$(INTDIR)\VirtualRegister.sbr"
	-@erase "$(INTDIR)\Win32BreakPoint.obj"
	-@erase "$(INTDIR)\Win32BreakPoint.sbr"
	-@erase "$(INTDIR)\Win32ExceptionHandler.obj"
	-@erase "$(INTDIR)\Win32ExceptionHandler.sbr"
	-@erase "$(INTDIR)\x86-win32.nad.burg.obj"
	-@erase "$(INTDIR)\x86-win32.nad.burg.sbr"
	-@erase "$(INTDIR)\x86ArgumentList.obj"
	-@erase "$(INTDIR)\x86ArgumentList.sbr"
	-@erase "$(INTDIR)\x86ExceptionHandler.obj"
	-@erase "$(INTDIR)\x86ExceptionHandler.sbr"
	-@erase "$(INTDIR)\x86Float.obj"
	-@erase "$(INTDIR)\x86Float.sbr"
	-@erase "$(INTDIR)\x86Formatter.obj"
	-@erase "$(INTDIR)\x86Formatter.sbr"
	-@erase "$(INTDIR)\x86NativeMethodStubs.obj"
	-@erase "$(INTDIR)\x86NativeMethodStubs.sbr"
	-@erase "$(INTDIR)\x86Opcode.obj"
	-@erase "$(INTDIR)\x86Opcode.sbr"
	-@erase "$(INTDIR)\x86StdCall.obj"
	-@erase "$(INTDIR)\x86StdCall.sbr"
	-@erase "$(INTDIR)\x86SysCallsRuntime.obj"
	-@erase "$(INTDIR)\x86SysCallsRuntime.sbr"
	-@erase "$(INTDIR)\x86Win32_Support.obj"
	-@erase "$(INTDIR)\x86Win32_Support.sbr"
	-@erase "$(INTDIR)\x86Win32Emitter.obj"
	-@erase "$(INTDIR)\x86Win32Emitter.sbr"
	-@erase "$(INTDIR)\x86Win32Instruction.obj"
	-@erase "$(INTDIR)\x86Win32Instruction.sbr"
	-@erase "$(INTDIR)\x86Win32InvokeNative.obj"
	-@erase "$(INTDIR)\x86Win32InvokeNative.sbr"
	-@erase "$(INTDIR)\x86Win32Thread.obj"
	-@erase "$(INTDIR)\x86Win32Thread.sbr"
	-@erase "$(INTDIR)\XDisAsm.obj"
	-@erase "$(INTDIR)\XDisAsm.sbr"
	-@erase "$(INTDIR)\XDisAsmTable.obj"
	-@erase "$(INTDIR)\XDisAsmTable.sbr"
	-@erase "$(INTDIR)\ZipArchive.obj"
	-@erase "$(INTDIR)\ZipArchive.sbr"
	-@erase "$(INTDIR)\ZipFileReader.obj"
	-@erase "$(INTDIR)\ZipFileReader.sbr"
	-@erase "$(INTDIR)\zutil.obj"
	-@erase "$(INTDIR)\zutil.sbr"
	-@erase "$(OUTDIR)\electricalfire.bsc"
	-@erase "$(OUTDIR)\electricalfire.dll"
	-@erase "$(OUTDIR)\electricalfire.exp"
	-@erase "$(OUTDIR)\electricalfire.ilk"
	-@erase "$(OUTDIR)\electricalfire.lib"
	-@erase "$(OUTDIR)\electricalfire.pdb"
	-@erase "genfiles\$(InputName).cpp"
	-@erase "genfiles\$(InputName).h"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /I "..\..\..\Exports" /I\
 "..\..\..\Exports\md" /I "..\..\..\Utilities\General" /I\
 "..\..\..\Utilities\DisAssemblers\x86" /I "..\..\..\Utilities\General\md" /I\
 "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I\
 "..\..\..\Runtime\System\md\x86" /I\
 "..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I\
 "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I\
 "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I\
 "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I\
 "..\..\..\Compiler\RegisterAllocator" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86" /I\
 "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I\
 "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I\
 "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I\
 "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "DEBUG" /D\
 "DEBUG_LOG" /D "_DEBUG" /D "DEBUG_DESANTIS" /D "DEBUG_kini" /D "EF_WINDOWS" /D\
 "GENERATE_FOR_X86" /D "XP_PC" /D "DLL_BUILD" /D "WIN32" /D "_CONSOLE" /D\
 "_MBCS" /D "GENERATE_NATIVE_STUBS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\electric/
CPP_SBRS=.\electric/
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\electricalfire.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\Address.sbr" \
	"$(INTDIR)\adler32.sbr" \
	"$(INTDIR)\AttributeHandlers.sbr" \
	"$(INTDIR)\Backend.sbr" \
	"$(INTDIR)\BufferedFileReader.sbr" \
	"$(INTDIR)\BytecodeGraph.sbr" \
	"$(INTDIR)\BytecodeTranslator.sbr" \
	"$(INTDIR)\BytecodeVerifier.sbr" \
	"$(INTDIR)\CatchAssert.sbr" \
	"$(INTDIR)\CGScheduler.sbr" \
	"$(INTDIR)\ClassCentral.sbr" \
	"$(INTDIR)\ClassFileSummary.sbr" \
	"$(INTDIR)\ClassReader.sbr" \
	"$(INTDIR)\ClassWorld.sbr" \
	"$(INTDIR)\CodeGenerator.sbr" \
	"$(INTDIR)\Coloring.sbr" \
	"$(INTDIR)\compress.sbr" \
	"$(INTDIR)\ControlGraph.sbr" \
	"$(INTDIR)\ControlNodes.sbr" \
	"$(INTDIR)\crc32.sbr" \
	"$(INTDIR)\CUtils.sbr" \
	"$(INTDIR)\Debugger.sbr" \
	"$(INTDIR)\DebugUtils.sbr" \
	"$(INTDIR)\deflate.sbr" \
	"$(INTDIR)\DiskFileReader.sbr" \
	"$(INTDIR)\ErrorHandling.sbr" \
	"$(INTDIR)\ExceptionTable.sbr" \
	"$(INTDIR)\FastBitMatrix.sbr" \
	"$(INTDIR)\FastBitSet.sbr" \
	"$(INTDIR)\FieldOrMethod.sbr" \
	"$(INTDIR)\FileReader.sbr" \
	"$(INTDIR)\FloatUtils.sbr" \
	"$(INTDIR)\gzio.sbr" \
	"$(INTDIR)\HTMLMethodDump.sbr" \
	"$(INTDIR)\IGVisualizer.sbr" \
	"$(INTDIR)\infblock.sbr" \
	"$(INTDIR)\infcodes.sbr" \
	"$(INTDIR)\inffast.sbr" \
	"$(INTDIR)\inflate.sbr" \
	"$(INTDIR)\inftrees.sbr" \
	"$(INTDIR)\infutil.sbr" \
	"$(INTDIR)\Instruction.sbr" \
	"$(INTDIR)\InstructionEmitter.sbr" \
	"$(INTDIR)\InterestingEvents.sbr" \
	"$(INTDIR)\InterfaceDispatchTable.sbr" \
	"$(INTDIR)\JavaBytecodes.sbr" \
	"$(INTDIR)\JavaObject.sbr" \
	"$(INTDIR)\JavaString.sbr" \
	"$(INTDIR)\JavaVM.sbr" \
	"$(INTDIR)\Jni.sbr" \
	"$(INTDIR)\JNIManglers.sbr" \
	"$(INTDIR)\jvmdi.sbr" \
	"$(INTDIR)\LogModule.sbr" \
	"$(INTDIR)\Method.sbr" \
	"$(INTDIR)\Monitor.sbr" \
	"$(INTDIR)\NameMangler.sbr" \
	"$(INTDIR)\NativeCodeCache.sbr" \
	"$(INTDIR)\NativeFormatter.sbr" \
	"$(INTDIR)\NativeMethodDispatcher.sbr" \
	"$(INTDIR)\NetscapeManglers.sbr" \
	"$(INTDIR)\PathComponent.sbr" \
	"$(INTDIR)\Pool.sbr" \
	"$(INTDIR)\PrimitiveBuilders.sbr" \
	"$(INTDIR)\PrimitiveGraphVerifier.sbr" \
	"$(INTDIR)\PrimitiveOptimizer.sbr" \
	"$(INTDIR)\Primitives.sbr" \
	"$(INTDIR)\RegisterAllocator.sbr" \
	"$(INTDIR)\Scheduler.sbr" \
	"$(INTDIR)\Spilling.sbr" \
	"$(INTDIR)\StackWalker.sbr" \
	"$(INTDIR)\SysCalls.sbr" \
	"$(INTDIR)\SysCallsRuntime.sbr" \
	"$(INTDIR)\Thread.sbr" \
	"$(INTDIR)\TranslationEnv.sbr" \
	"$(INTDIR)\Tree.sbr" \
	"$(INTDIR)\trees.sbr" \
	"$(INTDIR)\uncompr.sbr" \
	"$(INTDIR)\Value.sbr" \
	"$(INTDIR)\VerificationEnv.sbr" \
	"$(INTDIR)\VirtualRegister.sbr" \
	"$(INTDIR)\Win32BreakPoint.sbr" \
	"$(INTDIR)\Win32ExceptionHandler.sbr" \
	"$(INTDIR)\x86-win32.nad.burg.sbr" \
	"$(INTDIR)\x86ArgumentList.sbr" \
	"$(INTDIR)\x86ExceptionHandler.sbr" \
	"$(INTDIR)\x86Float.sbr" \
	"$(INTDIR)\x86Formatter.sbr" \
	"$(INTDIR)\x86NativeMethodStubs.sbr" \
	"$(INTDIR)\x86Opcode.sbr" \
	"$(INTDIR)\x86StdCall.sbr" \
	"$(INTDIR)\x86SysCallsRuntime.sbr" \
	"$(INTDIR)\x86Win32_Support.sbr" \
	"$(INTDIR)\x86Win32Emitter.sbr" \
	"$(INTDIR)\x86Win32Instruction.sbr" \
	"$(INTDIR)\x86Win32InvokeNative.sbr" \
	"$(INTDIR)\x86Win32Thread.sbr" \
	"$(INTDIR)\XDisAsm.sbr" \
	"$(INTDIR)\XDisAsmTable.sbr" \
	"$(INTDIR)\ZipArchive.sbr" \
	"$(INTDIR)\ZipFileReader.sbr" \
	"$(INTDIR)\zutil.sbr"

"$(OUTDIR)\electricalfire.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=electric\DebuggerChannel.lib\
 ..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\lib\libnspr21.lib\
 ..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\lib\libplc21_s.lib wsock32.lib\
 kernel32.lib user32.lib /nologo /subsystem:console /dll /incremental:yes\
 /pdb:"$(OUTDIR)\electricalfire.pdb" /debug /machine:I386 /nodefaultlib:"MSVCRT"\
 /out:"$(OUTDIR)\electricalfire.dll" /implib:"$(OUTDIR)\electricalfire.lib"\
 /pdbtype:sept /libpath:"..\..\..\..\..\dist\WINNT4.0_DBG.OBJ\lib" 
LINK32_OBJS= \
	"$(INTDIR)\Address.obj" \
	"$(INTDIR)\adler32.obj" \
	"$(INTDIR)\AttributeHandlers.obj" \
	"$(INTDIR)\Backend.obj" \
	"$(INTDIR)\BufferedFileReader.obj" \
	"$(INTDIR)\BytecodeGraph.obj" \
	"$(INTDIR)\BytecodeTranslator.obj" \
	"$(INTDIR)\BytecodeVerifier.obj" \
	"$(INTDIR)\CatchAssert.obj" \
	"$(INTDIR)\CGScheduler.obj" \
	"$(INTDIR)\ClassCentral.obj" \
	"$(INTDIR)\ClassFileSummary.obj" \
	"$(INTDIR)\ClassReader.obj" \
	"$(INTDIR)\ClassWorld.obj" \
	"$(INTDIR)\CodeGenerator.obj" \
	"$(INTDIR)\Coloring.obj" \
	"$(INTDIR)\compress.obj" \
	"$(INTDIR)\ControlGraph.obj" \
	"$(INTDIR)\ControlNodes.obj" \
	"$(INTDIR)\crc32.obj" \
	"$(INTDIR)\CUtils.obj" \
	"$(INTDIR)\Debugger.obj" \
	"$(INTDIR)\DebugUtils.obj" \
	"$(INTDIR)\deflate.obj" \
	"$(INTDIR)\DiskFileReader.obj" \
	"$(INTDIR)\ErrorHandling.obj" \
	"$(INTDIR)\ExceptionTable.obj" \
	"$(INTDIR)\FastBitMatrix.obj" \
	"$(INTDIR)\FastBitSet.obj" \
	"$(INTDIR)\FieldOrMethod.obj" \
	"$(INTDIR)\FileReader.obj" \
	"$(INTDIR)\FloatUtils.obj" \
	"$(INTDIR)\gzio.obj" \
	"$(INTDIR)\HTMLMethodDump.obj" \
	"$(INTDIR)\IGVisualizer.obj" \
	"$(INTDIR)\infblock.obj" \
	"$(INTDIR)\infcodes.obj" \
	"$(INTDIR)\inffast.obj" \
	"$(INTDIR)\inflate.obj" \
	"$(INTDIR)\inftrees.obj" \
	"$(INTDIR)\infutil.obj" \
	"$(INTDIR)\Instruction.obj" \
	"$(INTDIR)\InstructionEmitter.obj" \
	"$(INTDIR)\InterestingEvents.obj" \
	"$(INTDIR)\InterfaceDispatchTable.obj" \
	"$(INTDIR)\JavaBytecodes.obj" \
	"$(INTDIR)\JavaObject.obj" \
	"$(INTDIR)\JavaString.obj" \
	"$(INTDIR)\JavaVM.obj" \
	"$(INTDIR)\Jni.obj" \
	"$(INTDIR)\JNIManglers.obj" \
	"$(INTDIR)\jvmdi.obj" \
	"$(INTDIR)\LogModule.obj" \
	"$(INTDIR)\Method.obj" \
	"$(INTDIR)\Monitor.obj" \
	"$(INTDIR)\NameMangler.obj" \
	"$(INTDIR)\NativeCodeCache.obj" \
	"$(INTDIR)\NativeFormatter.obj" \
	"$(INTDIR)\NativeMethodDispatcher.obj" \
	"$(INTDIR)\NetscapeManglers.obj" \
	"$(INTDIR)\PathComponent.obj" \
	"$(INTDIR)\Pool.obj" \
	"$(INTDIR)\PrimitiveBuilders.obj" \
	"$(INTDIR)\PrimitiveGraphVerifier.obj" \
	"$(INTDIR)\PrimitiveOptimizer.obj" \
	"$(INTDIR)\Primitives.obj" \
	"$(INTDIR)\RegisterAllocator.obj" \
	"$(INTDIR)\Scheduler.obj" \
	"$(INTDIR)\Spilling.obj" \
	"$(INTDIR)\StackWalker.obj" \
	"$(INTDIR)\SysCalls.obj" \
	"$(INTDIR)\SysCallsRuntime.obj" \
	"$(INTDIR)\Thread.obj" \
	"$(INTDIR)\TranslationEnv.obj" \
	"$(INTDIR)\Tree.obj" \
	"$(INTDIR)\trees.obj" \
	"$(INTDIR)\uncompr.obj" \
	"$(INTDIR)\Value.obj" \
	"$(INTDIR)\VerificationEnv.obj" \
	"$(INTDIR)\VirtualRegister.obj" \
	"$(INTDIR)\Win32BreakPoint.obj" \
	"$(INTDIR)\Win32ExceptionHandler.obj" \
	"$(INTDIR)\x86-win32.nad.burg.obj" \
	"$(INTDIR)\x86ArgumentList.obj" \
	"$(INTDIR)\x86ExceptionHandler.obj" \
	"$(INTDIR)\x86Float.obj" \
	"$(INTDIR)\x86Formatter.obj" \
	"$(INTDIR)\x86NativeMethodStubs.obj" \
	"$(INTDIR)\x86Opcode.obj" \
	"$(INTDIR)\x86StdCall.obj" \
	"$(INTDIR)\x86SysCallsRuntime.obj" \
	"$(INTDIR)\x86Win32_Support.obj" \
	"$(INTDIR)\x86Win32Emitter.obj" \
	"$(INTDIR)\x86Win32Instruction.obj" \
	"$(INTDIR)\x86Win32InvokeNative.obj" \
	"$(INTDIR)\x86Win32Thread.obj" \
	"$(INTDIR)\XDisAsm.obj" \
	"$(INTDIR)\XDisAsmTable.obj" \
	"$(INTDIR)\ZipArchive.obj" \
	"$(INTDIR)\ZipFileReader.obj" \
	"$(INTDIR)\zutil.obj" \
	"$(OUTDIR)\DebuggerChannel.lib"

"$(OUTDIR)\electricalfire.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL_PROJ=

!IF "$(CFG)" == "electricalfire - Win32 Release" || "$(CFG)" ==\
 "electricalfire - Win32 Debug"
SOURCE=".\genfiles\x86-win32.nad.burg.cpp"

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86_W=\
	"..\..\..\Compiler\CodeGenerator\Burg.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_X86_W=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86-win32.nad.burg.obj"	"$(INTDIR)\x86-win32.nad.burg.sbr" : \
$(SOURCE) $(DEP_CPP_X86_W) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86_W=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Burg.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\x86-win32.nad.burg.obj"	"$(INTDIR)\x86-win32.nad.burg.sbr" : \
$(SOURCE) $(DEP_CPP_X86_W) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=".\genfiles\ppc601-macos.nad.burg.cpp"
SOURCE=..\..\..\Runtime\ClassInfo\ClassCentral.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CLASS=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\DiskFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_CLASS=\
	"..\..\..\Runtime\ClassInfo\plstr.h"\
	"..\..\..\Runtime\ClassInfo\prenv.h"\
	"..\..\..\Runtime\ClassInfo\prio.h"\
	"..\..\..\Runtime\ClassInfo\prprf.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prio.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\ClassCentral.obj"	"$(INTDIR)\ClassCentral.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CLASS=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\DiskFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\ClassCentral.obj"	"$(INTDIR)\ClassCentral.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\ClassInfo\ClassFileSummary.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CLASSF=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_CLASSF=\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassInfo\plstr.h"\
	"..\..\..\Runtime\ClassInfo\prprf.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ClassFileSummary.obj"	"$(INTDIR)\ClassFileSummary.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CLASSF=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\ClassFileSummary.obj"	"$(INTDIR)\ClassFileSummary.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\ClassInfo\PathComponent.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_PATHC=\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\FileReader\DiskFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\FileReader\ZipFileReader.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_PATHC=\
	"..\..\..\Runtime\ClassInfo\plstr.h"\
	"..\..\..\Runtime\ClassInfo\prio.h"\
	"..\..\..\Runtime\ClassInfo\prprf.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prio.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\PathComponent.obj"	"$(INTDIR)\PathComponent.sbr" : $(SOURCE)\
 $(DEP_CPP_PATHC) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_PATHC=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\FileReader\DiskFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\FileReader\ZipFileReader.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\PathComponent.obj"	"$(INTDIR)\PathComponent.sbr" : $(SOURCE)\
 $(DEP_CPP_PATHC) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\ClassReader\AttributeHandlers.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_ATTRI=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\AttributeHandlers.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_ATTRI=\
	"..\..\..\Runtime\ClassReader\plstr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\AttributeHandlers.obj"	"$(INTDIR)\AttributeHandlers.sbr" : $(SOURCE)\
 $(DEP_CPP_ATTRI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_ATTRI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\AttributeHandlers.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\AttributeHandlers.obj"	"$(INTDIR)\AttributeHandlers.sbr" : $(SOURCE)\
 $(DEP_CPP_ATTRI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\ClassReader\ClassReader.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CLASSR=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\AttributeHandlers.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_CLASSR=\
	"..\..\..\Runtime\ClassReader\plstr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ClassReader.obj"	"$(INTDIR)\ClassReader.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CLASSR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\AttributeHandlers.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\ClassReader.obj"	"$(INTDIR)\ClassReader.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\FileReader\BufferedFileReader.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_BUFFE=\
	"..\..\..\Runtime\FileReader\BufferedFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	
NODEP_CPP_BUFFE=\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\BufferedFileReader.obj"	"$(INTDIR)\BufferedFileReader.sbr" : \
$(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_BUFFE=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\FileReader\BufferedFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\BufferedFileReader.obj"	"$(INTDIR)\BufferedFileReader.sbr" : \
$(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\FileReader\DiskFileReader.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_DISKF=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Runtime\FileReader\DiskFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_DISKF=\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\DiskFileReader.obj"	"$(INTDIR)\DiskFileReader.sbr" : $(SOURCE)\
 $(DEP_CPP_DISKF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_DISKF=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Runtime\FileReader\DiskFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\DiskFileReader.obj"	"$(INTDIR)\DiskFileReader.sbr" : $(SOURCE)\
 $(DEP_CPP_DISKF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\FileReader\FileReader.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_FILER=\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	
NODEP_CPP_FILER=\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\FileReader.obj"	"$(INTDIR)\FileReader.sbr" : $(SOURCE)\
 $(DEP_CPP_FILER) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_FILER=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\FileReader.obj"	"$(INTDIR)\FileReader.sbr" : $(SOURCE)\
 $(DEP_CPP_FILER) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\FileReader\ZipArchive.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_ZIPAR=\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_ZIPAR=\
	"..\..\..\Runtime\FileReader\plstr.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prio.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\ZipArchive.obj"	"$(INTDIR)\ZipArchive.sbr" : $(SOURCE)\
 $(DEP_CPP_ZIPAR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_ZIPAR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\ZipArchive.obj"	"$(INTDIR)\ZipArchive.sbr" : $(SOURCE)\
 $(DEP_CPP_ZIPAR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\FileReader\ZipFileReader.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_ZIPFI=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\FileReader\ZipFileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_ZIPFI=\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ZipFileReader.obj"	"$(INTDIR)\ZipFileReader.sbr" : $(SOURCE)\
 $(DEP_CPP_ZIPFI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_ZIPFI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\FileReader\ZipFileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\ZipFileReader.obj"	"$(INTDIR)\ZipFileReader.sbr" : $(SOURCE)\
 $(DEP_CPP_ZIPFI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\NativeMethods\md\x86\x86NativeMethodStubs.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86NA=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodStubs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86NA=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86NativeMethodStubs.obj"	"$(INTDIR)\x86NativeMethodStubs.sbr" : \
$(SOURCE) $(DEP_CPP_X86NA) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86NA=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodStubs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86NativeMethodStubs.obj"	"$(INTDIR)\x86NativeMethodStubs.sbr" : \
$(SOURCE) $(DEP_CPP_X86NA) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\NativeMethods\Jni.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JNI_C=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\BufferedFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_JNI_C=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\NativeMethods\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Jni.obj"	"$(INTDIR)\Jni.sbr" : $(SOURCE) $(DEP_CPP_JNI_C)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JNI_C=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\BufferedFileReader.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\Jni.obj"	"$(INTDIR)\Jni.sbr" : $(SOURCE) $(DEP_CPP_JNI_C)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\NativeMethods\JNIManglers.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JNIMA=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JNIManglers.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodStubs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_JNIMA=\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\plstr.h"\
	"..\..\..\Runtime\NativeMethods\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\JNIManglers.obj"	"$(INTDIR)\JNIManglers.sbr" : $(SOURCE)\
 $(DEP_CPP_JNIMA) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JNIMA=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JNIManglers.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodStubs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\JNIManglers.obj"	"$(INTDIR)\JNIManglers.sbr" : $(SOURCE)\
 $(DEP_CPP_JNIMA) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\NativeMethods\NameMangler.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_NAMEM=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_NAMEM=\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\plstr.h"\
	"..\..\..\Runtime\NativeMethods\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\NameMangler.obj"	"$(INTDIR)\NameMangler.sbr" : $(SOURCE)\
 $(DEP_CPP_NAMEM) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_NAMEM=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\NameMangler.obj"	"$(INTDIR)\NameMangler.sbr" : $(SOURCE)\
 $(DEP_CPP_NAMEM) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_NATIV=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JNIManglers.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\NativeMethods\NetscapeManglers.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_NATIV=\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\plstr.h"\
	"..\..\..\Runtime\NativeMethods\prio.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\NativeMethods\prmem.h"\
	"..\..\..\Runtime\NativeMethods\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prio.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\NativeMethodDispatcher.obj"	"$(INTDIR)\NativeMethodDispatcher.sbr" :\
 $(SOURCE) $(DEP_CPP_NATIV) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_NATIV=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JNIManglers.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\NativeMethods\NetscapeManglers.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\NativeMethodDispatcher.obj"	"$(INTDIR)\NativeMethodDispatcher.sbr" :\
 $(SOURCE) $(DEP_CPP_NATIV) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\NativeMethods\NetscapeManglers.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_NETSC=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JNIManglers.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NetscapeManglers.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_NETSC=\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\plstr.h"\
	"..\..\..\Runtime\NativeMethods\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\NetscapeManglers.obj"	"$(INTDIR)\NetscapeManglers.sbr" : $(SOURCE)\
 $(DEP_CPP_NETSC) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_NETSC=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JNIManglers.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NetscapeManglers.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\NetscapeManglers.obj"	"$(INTDIR)\NetscapeManglers.sbr" : $(SOURCE)\
 $(DEP_CPP_NETSC) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\md\x86\Win32ExceptionHandler.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_WIN32=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_WIN32=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\md\x86\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Win32ExceptionHandler.obj"	"$(INTDIR)\Win32ExceptionHandler.sbr" : \
$(SOURCE) $(DEP_CPP_WIN32) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_WIN32=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\Win32ExceptionHandler.obj"	"$(INTDIR)\Win32ExceptionHandler.sbr" : \
$(SOURCE) $(DEP_CPP_WIN32) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\md\x86\x86ExceptionHandler.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86EX=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\StackWalker.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86EX=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\md\x86\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86ExceptionHandler.obj"	"$(INTDIR)\x86ExceptionHandler.sbr" : \
$(SOURCE) $(DEP_CPP_X86EX) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86EX=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\StackWalker.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86ExceptionHandler.obj"	"$(INTDIR)\x86ExceptionHandler.sbr" : \
$(SOURCE) $(DEP_CPP_X86EX) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86SY=\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_X86SY=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86SysCallsRuntime.obj"	"$(INTDIR)\x86SysCallsRuntime.sbr" : \
$(SOURCE) $(DEP_CPP_X86SY) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86SY=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\x86SysCallsRuntime.obj"	"$(INTDIR)\x86SysCallsRuntime.sbr" : \
$(SOURCE) $(DEP_CPP_X86SY) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\md\x86\x86Win32InvokeNative.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86WI=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	
NODEP_CPP_X86WI=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Win32InvokeNative.obj"	"$(INTDIR)\x86Win32InvokeNative.sbr" : \
$(SOURCE) $(DEP_CPP_X86WI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86WI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	

"$(INTDIR)\x86Win32InvokeNative.obj"	"$(INTDIR)\x86Win32InvokeNative.sbr" : \
$(SOURCE) $(DEP_CPP_X86WI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\md\x86\x86Win32Thread.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86WIN=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86WIN=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\md\x86\prprf.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\pratom.h"\
	"..\..\..\Runtime\System\prcvar.h"\
	"..\..\..\Runtime\System\prlock.h"\
	"..\..\..\Runtime\System\prthread.h"\
	"..\..\..\Runtime\System\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Win32Thread.obj"	"$(INTDIR)\x86Win32Thread.sbr" : $(SOURCE)\
 $(DEP_CPP_X86WIN) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86WIN=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86Win32Thread.obj"	"$(INTDIR)\x86Win32Thread.sbr" : $(SOURCE)\
 $(DEP_CPP_X86WIN) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\ClassWorld.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CLASSW=\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_CLASSW=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ClassWorld.obj"	"$(INTDIR)\ClassWorld.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSW) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CLASSW=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\ClassWorld.obj"	"$(INTDIR)\ClassWorld.sbr" : $(SOURCE)\
 $(DEP_CPP_CLASSW) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\FieldOrMethod.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_FIELD=\
	"..\..\..\Compiler\CodeGenerator\Backend.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeTranslator.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\md\FieldOrMethod_md.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_FIELD=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prio.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\FieldOrMethod.obj"	"$(INTDIR)\FieldOrMethod.sbr" : $(SOURCE)\
 $(DEP_CPP_FIELD) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_FIELD=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Backend.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeTranslator.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\md\FieldOrMethod_md.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\FieldOrMethod.obj"	"$(INTDIR)\FieldOrMethod.sbr" : $(SOURCE)\
 $(DEP_CPP_FIELD) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\InterfaceDispatchTable.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INTER=\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_INTER=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\InterfaceDispatchTable.obj"	"$(INTDIR)\InterfaceDispatchTable.sbr" :\
 $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INTER=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\InterfaceDispatchTable.obj"	"$(INTDIR)\InterfaceDispatchTable.sbr" :\
 $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\JavaObject.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JAVAO=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_JAVAO=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\JavaObject.obj"	"$(INTDIR)\JavaObject.sbr" : $(SOURCE)\
 $(DEP_CPP_JAVAO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JAVAO=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\JavaObject.obj"	"$(INTDIR)\JavaObject.sbr" : $(SOURCE)\
 $(DEP_CPP_JAVAO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\JavaString.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JAVAS=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_JAVAS=\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\JavaString.obj"	"$(INTDIR)\JavaString.sbr" : $(SOURCE)\
 $(DEP_CPP_JAVAS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JAVAS=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\JavaString.obj"	"$(INTDIR)\JavaString.sbr" : $(SOURCE)\
 $(DEP_CPP_JAVAS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\JavaVM.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JAVAV=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Debugger\jvmdi.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JniRuntime.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_JAVAV=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\pratom.h"\
	"..\..\..\Runtime\System\prcvar.h"\
	"..\..\..\Runtime\System\prlock.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Runtime\System\prthread.h"\
	"..\..\..\Runtime\System\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\JavaVM.obj"	"$(INTDIR)\JavaVM.sbr" : $(SOURCE) $(DEP_CPP_JAVAV)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JAVAV=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Debugger\jvmdi.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\JniRuntime.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\JavaVM.obj"	"$(INTDIR)\JavaVM.sbr" : $(SOURCE) $(DEP_CPP_JAVAV)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\Method.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_METHO=\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_METHO=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Method.obj"	"$(INTDIR)\Method.sbr" : $(SOURCE) $(DEP_CPP_METHO)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_METHO=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Method.obj"	"$(INTDIR)\Method.sbr" : $(SOURCE) $(DEP_CPP_METHO)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\Monitor.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_MONIT=\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_MONIT=\
	"..\..\..\Runtime\System\pratom.h"\
	"..\..\..\Runtime\System\prcvar.h"\
	"..\..\..\Runtime\System\prlock.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Runtime\System\prthread.h"\
	"..\..\..\Runtime\System\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Monitor.obj"	"$(INTDIR)\Monitor.sbr" : $(SOURCE) $(DEP_CPP_MONIT)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_MONIT=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Monitor.obj"	"$(INTDIR)\Monitor.sbr" : $(SOURCE) $(DEP_CPP_MONIT)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\StackWalker.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_STACK=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\StackWalker.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_STACK=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\StackWalker.obj"	"$(INTDIR)\StackWalker.sbr" : $(SOURCE)\
 $(DEP_CPP_STACK) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_STACK=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\StackWalker.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\StackWalker.obj"	"$(INTDIR)\StackWalker.sbr" : $(SOURCE)\
 $(DEP_CPP_STACK) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\SysCallsRuntime.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_SYSCA=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_SYSCA=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\pratom.h"\
	"..\..\..\Runtime\System\prcvar.h"\
	"..\..\..\Runtime\System\prlock.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Runtime\System\prthread.h"\
	"..\..\..\Runtime\System\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\SysCallsRuntime.obj"	"$(INTDIR)\SysCallsRuntime.sbr" : $(SOURCE)\
 $(DEP_CPP_SYSCA) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_SYSCA=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\SysCallsRuntime.obj"	"$(INTDIR)\SysCallsRuntime.sbr" : $(SOURCE)\
 $(DEP_CPP_SYSCA) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Runtime\System\Thread.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_THREA=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86LinuxThread.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32Thread.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_THREA=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\pratom.h"\
	"..\..\..\Runtime\System\prcvar.h"\
	"..\..\..\Runtime\System\prlock.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Runtime\System\prthread.h"\
	"..\..\..\Runtime\System\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Thread.obj"	"$(INTDIR)\Thread.sbr" : $(SOURCE) $(DEP_CPP_THREA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_THREA=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\Exceptions.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32Thread.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\Thread.obj"	"$(INTDIR)\Thread.sbr" : $(SOURCE) $(DEP_CPP_THREA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\CatchAssert.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CATCH=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\md\CatchAssert_md.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_CATCH=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\CatchAssert.obj"	"$(INTDIR)\CatchAssert.sbr" : $(SOURCE)\
 $(DEP_CPP_CATCH) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CATCH=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\md\CatchAssert_md.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\CatchAssert.obj"	"$(INTDIR)\CatchAssert.sbr" : $(SOURCE)\
 $(DEP_CPP_CATCH) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\CUtils.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CUTIL=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	
NODEP_CPP_CUTIL=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prio.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\CUtils.obj"	"$(INTDIR)\CUtils.sbr" : $(SOURCE) $(DEP_CPP_CUTIL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CUTIL=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\CUtils.obj"	"$(INTDIR)\CUtils.sbr" : $(SOURCE) $(DEP_CPP_CUTIL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\DebugUtils.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_DEBUG=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	
NODEP_CPP_DEBUG=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\DebugUtils.obj"	"$(INTDIR)\DebugUtils.sbr" : $(SOURCE)\
 $(DEP_CPP_DEBUG) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_DEBUG=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	

"$(INTDIR)\DebugUtils.obj"	"$(INTDIR)\DebugUtils.sbr" : $(SOURCE)\
 $(DEP_CPP_DEBUG) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\FastBitMatrix.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_FASTB=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	
NODEP_CPP_FASTB=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\FastBitMatrix.obj"	"$(INTDIR)\FastBitMatrix.sbr" : $(SOURCE)\
 $(DEP_CPP_FASTB) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_FASTB=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\FastBitMatrix.obj"	"$(INTDIR)\FastBitMatrix.sbr" : $(SOURCE)\
 $(DEP_CPP_FASTB) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\FastBitSet.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_FASTBI=\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_FASTBI=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\FastBitSet.obj"	"$(INTDIR)\FastBitSet.sbr" : $(SOURCE)\
 $(DEP_CPP_FASTBI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_FASTBI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\FastBitSet.obj"	"$(INTDIR)\FastBitSet.sbr" : $(SOURCE)\
 $(DEP_CPP_FASTBI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\FloatUtils.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_FLOAT=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	
NODEP_CPP_FLOAT=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\FloatUtils.obj"	"$(INTDIR)\FloatUtils.sbr" : $(SOURCE)\
 $(DEP_CPP_FLOAT) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_FLOAT=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	

"$(INTDIR)\FloatUtils.obj"	"$(INTDIR)\FloatUtils.sbr" : $(SOURCE)\
 $(DEP_CPP_FLOAT) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\InterestingEvents.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INTERE=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_INTERE=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\InterestingEvents.obj"	"$(INTDIR)\InterestingEvents.sbr" : $(SOURCE)\
 $(DEP_CPP_INTERE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INTERE=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\InterestingEvents.obj"	"$(INTDIR)\InterestingEvents.sbr" : $(SOURCE)\
 $(DEP_CPP_INTERE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\JavaBytecodes.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JAVAB=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_JAVAB=\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\JavaBytecodes.obj"	"$(INTDIR)\JavaBytecodes.sbr" : $(SOURCE)\
 $(DEP_CPP_JAVAB) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JAVAB=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\JavaBytecodes.obj"	"$(INTDIR)\JavaBytecodes.sbr" : $(SOURCE)\
 $(DEP_CPP_JAVAB) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\LogModule.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_LOGMO=\
	"..\..\..\Utilities\General\LogModule.h"\
	
NODEP_CPP_LOGMO=\
	"..\..\..\Utilities\General\prlog.h"\
	

"$(INTDIR)\LogModule.obj"	"$(INTDIR)\LogModule.sbr" : $(SOURCE)\
 $(DEP_CPP_LOGMO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_LOGMO=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	

"$(INTDIR)\LogModule.obj"	"$(INTDIR)\LogModule.sbr" : $(SOURCE)\
 $(DEP_CPP_LOGMO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\Pool.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_POOL_=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	
NODEP_CPP_POOL_=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Pool.obj"	"$(INTDIR)\Pool.sbr" : $(SOURCE) $(DEP_CPP_POOL_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_POOL_=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\Pool.obj"	"$(INTDIR)\Pool.sbr" : $(SOURCE) $(DEP_CPP_POOL_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\General\Tree.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_TREE_=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Tree.h"\
	
NODEP_CPP_TREE_=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Tree.obj"	"$(INTDIR)\Tree.sbr" : $(SOURCE) $(DEP_CPP_TREE_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_TREE_=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Tree.h"\
	

"$(INTDIR)\Tree.obj"	"$(INTDIR)\Tree.sbr" : $(SOURCE) $(DEP_CPP_TREE_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\adler32.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_ADLER=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\adler32.obj"	"$(INTDIR)\adler32.sbr" : $(SOURCE) $(DEP_CPP_ADLER)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_ADLER=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\adler32.obj"	"$(INTDIR)\adler32.sbr" : $(SOURCE) $(DEP_CPP_ADLER)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\compress.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_COMPR=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\compress.obj"	"$(INTDIR)\compress.sbr" : $(SOURCE) $(DEP_CPP_COMPR)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_COMPR=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\compress.obj"	"$(INTDIR)\compress.sbr" : $(SOURCE) $(DEP_CPP_COMPR)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\crc32.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CRC32=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\crc32.obj"	"$(INTDIR)\crc32.sbr" : $(SOURCE) $(DEP_CPP_CRC32)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CRC32=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\crc32.obj"	"$(INTDIR)\crc32.sbr" : $(SOURCE) $(DEP_CPP_CRC32)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\deflate.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_DEFLA=\
	"..\..\..\Utilities\zlib\deflate.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\deflate.obj"	"$(INTDIR)\deflate.sbr" : $(SOURCE) $(DEP_CPP_DEFLA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_DEFLA=\
	"..\..\..\Utilities\zlib\deflate.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\deflate.obj"	"$(INTDIR)\deflate.sbr" : $(SOURCE) $(DEP_CPP_DEFLA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\gzio.c
DEP_CPP_GZIO_=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\gzio.obj"	"$(INTDIR)\gzio.sbr" : $(SOURCE) $(DEP_CPP_GZIO_)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\Utilities\zlib\infblock.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INFBL=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\infblock.obj"	"$(INTDIR)\infblock.sbr" : $(SOURCE) $(DEP_CPP_INFBL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INFBL=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\infblock.obj"	"$(INTDIR)\infblock.sbr" : $(SOURCE) $(DEP_CPP_INFBL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\infcodes.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INFCO=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inffast.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\infcodes.obj"	"$(INTDIR)\infcodes.sbr" : $(SOURCE) $(DEP_CPP_INFCO)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INFCO=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inffast.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\infcodes.obj"	"$(INTDIR)\infcodes.sbr" : $(SOURCE) $(DEP_CPP_INFCO)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\inffast.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INFFA=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inffast.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\inffast.obj"	"$(INTDIR)\inffast.sbr" : $(SOURCE) $(DEP_CPP_INFFA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INFFA=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inffast.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\inffast.obj"	"$(INTDIR)\inffast.sbr" : $(SOURCE) $(DEP_CPP_INFFA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\inflate.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INFLA=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\inflate.obj"	"$(INTDIR)\inflate.sbr" : $(SOURCE) $(DEP_CPP_INFLA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INFLA=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\inflate.obj"	"$(INTDIR)\inflate.sbr" : $(SOURCE) $(DEP_CPP_INFLA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\inftrees.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INFTR=\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\inftrees.obj"	"$(INTDIR)\inftrees.sbr" : $(SOURCE) $(DEP_CPP_INFTR)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INFTR=\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\inftrees.obj"	"$(INTDIR)\inftrees.sbr" : $(SOURCE) $(DEP_CPP_INFTR)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\infutil.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INFUT=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\infutil.obj"	"$(INTDIR)\infutil.sbr" : $(SOURCE) $(DEP_CPP_INFUT)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INFUT=\
	"..\..\..\Utilities\zlib\infblock.h"\
	"..\..\..\Utilities\zlib\infcodes.h"\
	"..\..\..\Utilities\zlib\inftrees.h"\
	"..\..\..\Utilities\zlib\infutil.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\infutil.obj"	"$(INTDIR)\infutil.sbr" : $(SOURCE) $(DEP_CPP_INFUT)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\trees.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_TREES=\
	"..\..\..\Utilities\zlib\deflate.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\trees.obj"	"$(INTDIR)\trees.sbr" : $(SOURCE) $(DEP_CPP_TREES)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_TREES=\
	"..\..\..\Utilities\zlib\deflate.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\trees.obj"	"$(INTDIR)\trees.sbr" : $(SOURCE) $(DEP_CPP_TREES)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\uncompr.c

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_UNCOM=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\uncompr.obj"	"$(INTDIR)\uncompr.sbr" : $(SOURCE) $(DEP_CPP_UNCOM)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_UNCOM=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\uncompr.obj"	"$(INTDIR)\uncompr.sbr" : $(SOURCE) $(DEP_CPP_UNCOM)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\zlib\zutil.c
DEP_CPP_ZUTIL=\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	"..\..\..\Utilities\zlib\zutil.h"\
	

"$(INTDIR)\zutil.obj"	"$(INTDIR)\zutil.sbr" : $(SOURCE) $(DEP_CPP_ZUTIL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\Compiler\FrontEnd\BytecodeGraph.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_BYTEC=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeVerifier.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Tree.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_BYTEC=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\BytecodeGraph.obj"	"$(INTDIR)\BytecodeGraph.sbr" : $(SOURCE)\
 $(DEP_CPP_BYTEC) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_BYTEC=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeVerifier.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Tree.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\BytecodeGraph.obj"	"$(INTDIR)\BytecodeGraph.sbr" : $(SOURCE)\
 $(DEP_CPP_BYTEC) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeTranslator.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_BYTECO=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeTranslator.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_BYTECO=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\BytecodeTranslator.obj"	"$(INTDIR)\BytecodeTranslator.sbr" : \
$(SOURCE) $(DEP_CPP_BYTECO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_BYTECO=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeTranslator.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\BytecodeTranslator.obj"	"$(INTDIR)\BytecodeTranslator.sbr" : \
$(SOURCE) $(DEP_CPP_BYTECO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\FrontEnd\BytecodeVerifier.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_BYTECOD=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeVerifier.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Tree.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_BYTECOD=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\BytecodeVerifier.obj"	"$(INTDIR)\BytecodeVerifier.sbr" : $(SOURCE)\
 $(DEP_CPP_BYTECOD) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_BYTECOD=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeVerifier.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Tree.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\BytecodeVerifier.obj"	"$(INTDIR)\BytecodeVerifier.sbr" : $(SOURCE)\
 $(DEP_CPP_BYTECOD) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\FrontEnd\ErrorHandling.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_ERROR=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_ERROR=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ErrorHandling.obj"	"$(INTDIR)\ErrorHandling.sbr" : $(SOURCE)\
 $(DEP_CPP_ERROR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_ERROR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\ErrorHandling.obj"	"$(INTDIR)\ErrorHandling.sbr" : $(SOURCE)\
 $(DEP_CPP_ERROR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\FrontEnd\TranslationEnv.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_TRANS=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_TRANS=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\TranslationEnv.obj"	"$(INTDIR)\TranslationEnv.sbr" : $(SOURCE)\
 $(DEP_CPP_TRANS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_TRANS=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\TranslationEnv.obj"	"$(INTDIR)\TranslationEnv.sbr" : $(SOURCE)\
 $(DEP_CPP_TRANS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\FrontEnd\VerificationEnv.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_VERIF=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_VERIF=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\VerificationEnv.obj"	"$(INTDIR)\VerificationEnv.sbr" : $(SOURCE)\
 $(DEP_CPP_VERIF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_VERIF=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\VerificationEnv.obj"	"$(INTDIR)\VerificationEnv.sbr" : $(SOURCE)\
 $(DEP_CPP_VERIF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsm.c
DEP_CPP_XDISA=\
	"..\..\..\Utilities\Disassemblers\x86\XDisAsm.h"\
	

"$(INTDIR)\XDisAsm.obj"	"$(INTDIR)\XDisAsm.sbr" : $(SOURCE) $(DEP_CPP_XDISA)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\Utilities\Disassemblers\x86\XDisAsmTable.c

"$(INTDIR)\XDisAsmTable.obj"	"$(INTDIR)\XDisAsmTable.sbr" : $(SOURCE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\Compiler\CodeGenerator\md\x86\x86-win32.nad"

!IF  "$(CFG)" == "electricalfire - Win32 Release"

InputPath=..\..\..\Compiler\CodeGenerator\md\x86\x86-win32.nad
InputName=x86-win32

"genfiles\$(InputName).nad.burg.cpp"	"genfiles\$(InputName).nad.burg.h"\
	"genfiles\PrimitiveOperations.h"	"genfiles\PrimitiveOperations.cpp"	 : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(NSTOOLS)\perl5\perl ..\..\..\tools\nad\nad.pl  $(InputPath)\
                                                                         ..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations\
                                                                                                genfiles\PrimitiveOperations.h genfiles\PrimitiveOperations.cpp\
                                                                                                  genfiles\$(InputName).nad.burg.h | Burg\Release\burg -I >\
                                                                                              genfiles\$(InputName).nad.burg.cpp

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

InputPath=..\..\..\Compiler\CodeGenerator\md\x86\x86-win32.nad
InputName=x86-win32

"genfiles\$(InputName).nad.burg.cpp"	"genfiles\$(InputName).nad.burg.h"	 : \
$(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(NSTOOLS)\perl5\perl ..\..\..\tools\nad\nad.pl  $(InputPath)\
                                                                         ..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations\
                                                                                                genfiles\PrimitiveOperations.h genfiles\PrimitiveOperations.cpp\
                                                                                                  genfiles\$(InputName).nad.burg.h | Burg\Debug\burg -I >\
                                                                                              genfiles\$(InputName).nad.burg.cpp

!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86AR=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86AR=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86ArgumentList.obj"	"$(INTDIR)\x86ArgumentList.sbr" : $(SOURCE)\
 $(DEP_CPP_X86AR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86AR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86ArgumentList.obj"	"$(INTDIR)\x86ArgumentList.sbr" : $(SOURCE)\
 $(DEP_CPP_X86AR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Float.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86FL=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\compiler\codegenerator\md\x86\x86float.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86FL=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Float.obj"	"$(INTDIR)\x86Float.sbr" : $(SOURCE) $(DEP_CPP_X86FL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86FL=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\compiler\codegenerator\md\x86\x86float.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86Float.obj"	"$(INTDIR)\x86Float.sbr" : $(SOURCE) $(DEP_CPP_X86FL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86FO=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86FO=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\XDisAsm.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Formatter.obj"	"$(INTDIR)\x86Formatter.sbr" : $(SOURCE)\
 $(DEP_CPP_X86FO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86FO=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\Disassemblers\x86\XDisAsm.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86Formatter.obj"	"$(INTDIR)\x86Formatter.sbr" : $(SOURCE)\
 $(DEP_CPP_X86FO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86OP=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_X86OP=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Opcode.obj"	"$(INTDIR)\x86Opcode.sbr" : $(SOURCE)\
 $(DEP_CPP_X86OP) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86OP=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\x86Opcode.obj"	"$(INTDIR)\x86Opcode.sbr" : $(SOURCE)\
 $(DEP_CPP_X86OP) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86StdCall.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86ST=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\compiler\codegenerator\md\x86\x86stdcall.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86ST=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86StdCall.obj"	"$(INTDIR)\x86StdCall.sbr" : $(SOURCE)\
 $(DEP_CPP_X86ST) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86ST=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\compiler\codegenerator\md\x86\x86stdcall.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86StdCall.obj"	"$(INTDIR)\x86StdCall.sbr" : $(SOURCE)\
 $(DEP_CPP_X86ST) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32_Support.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86WIN3=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86WIN3=\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	
CPP_SWITCHES=/nologo /ML /W3 /GX /I "..\..\..\Exports" /I "..\..\..\Exports\md"\
 /I "..\..\..\Utilities\General" /I "..\..\..\Utilities\General\md" /I\
 "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I\
 "..\..\..\Runtime\System\md\x86" /I "..\..\..\..\dist\WINNT4.0_DBG.OBJ\include"\
 /I "..\..\..\Utilities\zlib" /I "..\..\..\Runtime\ClassInfo" /I\
 "..\..\..\Runtime\ClassReader" /I "..\..\..\Runtime\FileReader" /I\
 "..\..\..\Runtime\NativeMethods" /I "..\..\..\Runtime\Systems" /I\
 "..\..\..\Compiler\CodeGenerator" /I "..\..\..\Compiler\RegisterAllocator" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86" /I\
 "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I\
 "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I\
 "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I\
 "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "NDEBUG"\
 /D "EF_WINDOWS" /D "GENERATE_FOR_X86" /D "XP_PC" /D "DLL_BUILD" /D "WIN32" /D\
 "_CONSOLE" /D "_MBCS" /D "GENERATE_NATIVE_STUBS" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\electricalfire.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\x86Win32_Support.obj"	"$(INTDIR)\x86Win32_Support.sbr" : $(SOURCE)\
 $(DEP_CPP_X86WIN3) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86WIN3=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
CPP_SWITCHES=/nologo /MDd /W3 /Gm /GX /Zi /I "..\..\..\Exports" /I\
 "..\..\..\Exports\md" /I "..\..\..\Utilities\General" /I\
 "..\..\..\Utilities\DisAssemblers\x86" /I "..\..\..\Utilities\General\md" /I\
 "..\..\..\Compiler\FrontEnd" /I "..\..\..\Compiler\PrimitiveGraph" /I\
 "..\..\..\Runtime\System\md\x86" /I\
 "..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include" /I "..\..\..\Utilities\zlib" /I\
 "..\..\..\Runtime\ClassInfo" /I "..\..\..\Runtime\ClassReader" /I\
 "..\..\..\Runtime\FileReader" /I "..\..\..\Runtime\NativeMethods" /I\
 "..\..\..\Runtime\Systems" /I "..\..\..\Compiler\CodeGenerator" /I\
 "..\..\..\Compiler\RegisterAllocator" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86\x86dis" /I\
 "..\..\..\Compiler\CodeGenerator\md\x86" /I\
 "..\..\..\Compiler\CodeGenerator\md" /I ".\\" /I ".\genfiles" /I\
 "..\..\..\Compiler\Optimizer" /I "..\..\..\Driver\StandAloneJava" /I\
 "..\..\..\Runtime\System" /I "..\..\..\Debugger" /I\
 "..\..\..\Runtime\System\md" /I "..\..\..\Debugger\Communication" /D "DEBUG" /D\
 "DEBUG_LOG" /D "_DEBUG" /D "DEBUG_DESANTIS" /D "DEBUG_kini" /D "EF_WINDOWS" /D\
 "GENERATE_FOR_X86" /D "XP_PC" /D "DLL_BUILD" /D "WIN32" /D "_CONSOLE" /D\
 "_MBCS" /D "GENERATE_NATIVE_STUBS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\x86Win32_Support.obj"	"$(INTDIR)\x86Win32_Support.sbr" : $(SOURCE)\
 $(DEP_CPP_X86WIN3) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86WIN32=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\compiler\codegenerator\md\x86\x86stdcall.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	".\genfiles\x86-win32.nad.burg.h"\
	
NODEP_CPP_X86WIN32=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Win32Emitter.obj"	"$(INTDIR)\x86Win32Emitter.sbr" : $(SOURCE)\
 $(DEP_CPP_X86WIN32) "$(INTDIR)" ".\genfiles\x86-win32.nad.burg.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86WIN32=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\compiler\codegenerator\md\x86\x86stdcall.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.h"\
	"..\..\..\Runtime\System\md\x86\x86Win32ExceptionHandler.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	".\genfiles\x86-win32.nad.burg.h"\
	

"$(INTDIR)\x86Win32Emitter.obj"	"$(INTDIR)\x86Win32Emitter.sbr" : $(SOURCE)\
 $(DEP_CPP_X86WIN32) "$(INTDIR)" ".\genfiles\x86-win32.nad.burg.h"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_X86WIN32I=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_X86WIN32I=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\x86Win32Instruction.obj"	"$(INTDIR)\x86Win32Instruction.sbr" : \
$(SOURCE) $(DEP_CPP_X86WIN32I) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_X86WIN32I=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86ArgumentList.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\MemoryAccess.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\x86Win32Instruction.obj"	"$(INTDIR)\x86Win32Instruction.sbr" : \
$(SOURCE) $(DEP_CPP_X86WIN32I) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\md\generic\Generic_Support.cpp
SOURCE=..\..\..\Compiler\CodeGenerator\Backend.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_BACKE=\
	"..\..\..\Compiler\CodeGenerator\Backend.h"\
	"..\..\..\Compiler\CodeGenerator\CGScheduler.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\ControlNodeScheduler.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\IGVisualizer.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Coloring.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAllocator.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h"\
	"..\..\..\Compiler\RegisterAllocator\Spilling.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fifo.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_BACKE=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Backend.obj"	"$(INTDIR)\Backend.sbr" : $(SOURCE) $(DEP_CPP_BACKE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_BACKE=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Backend.h"\
	"..\..\..\Compiler\CodeGenerator\CGScheduler.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Coloring.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAllocator.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h"\
	"..\..\..\Compiler\RegisterAllocator\Spilling.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fifo.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\Backend.obj"	"$(INTDIR)\Backend.sbr" : $(SOURCE) $(DEP_CPP_BACKE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\CGScheduler.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CGSCH=\
	"..\..\..\Compiler\CodeGenerator\CGScheduler.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_CGSCH=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\CGScheduler.obj"	"$(INTDIR)\CGScheduler.sbr" : $(SOURCE)\
 $(DEP_CPP_CGSCH) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CGSCH=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\CGScheduler.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\CGScheduler.obj"	"$(INTDIR)\CGScheduler.sbr" : $(SOURCE)\
 $(DEP_CPP_CGSCH) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\CodeGenerator.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CODEG=\
	"..\..\..\Compiler\CodeGenerator\Burg.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\IGVisualizer.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\Scheduler.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_CODEG=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\CodeGenerator.obj"	"$(INTDIR)\CodeGenerator.sbr" : $(SOURCE)\
 $(DEP_CPP_CODEG) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CODEG=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Burg.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\Scheduler.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\CodeGenerator.obj"	"$(INTDIR)\CodeGenerator.sbr" : $(SOURCE)\
 $(DEP_CPP_CODEG) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\ExceptionTable.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_EXCEP=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_EXCEP=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ExceptionTable.obj"	"$(INTDIR)\ExceptionTable.sbr" : $(SOURCE)\
 $(DEP_CPP_EXCEP) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_EXCEP=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\ExceptionTable.obj"	"$(INTDIR)\ExceptionTable.sbr" : $(SOURCE)\
 $(DEP_CPP_EXCEP) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\HTMLMethodDump.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_HTMLM=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\HTMLMethodDump.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_HTMLM=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\HTMLMethodDump.obj"	"$(INTDIR)\HTMLMethodDump.sbr" : $(SOURCE)\
 $(DEP_CPP_HTMLM) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_HTMLM=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\HTMLMethodDump.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\HTMLMethodDump.obj"	"$(INTDIR)\HTMLMethodDump.sbr" : $(SOURCE)\
 $(DEP_CPP_HTMLM) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\IGVisualizer.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_IGVIS=\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\IGVisualizer.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_IGVIS=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\IGVisualizer.obj"	"$(INTDIR)\IGVisualizer.sbr" : $(SOURCE)\
 $(DEP_CPP_IGVIS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"


"$(INTDIR)\IGVisualizer.obj"	"$(INTDIR)\IGVisualizer.sbr" : $(SOURCE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\Instruction.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INSTR=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_INSTR=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Instruction.obj"	"$(INTDIR)\Instruction.sbr" : $(SOURCE)\
 $(DEP_CPP_INSTR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INSTR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Instruction.obj"	"$(INTDIR)\Instruction.sbr" : $(SOURCE)\
 $(DEP_CPP_INSTR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\InstructionEmitter.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_INSTRU=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_INSTRU=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\InstructionEmitter.obj"	"$(INTDIR)\InstructionEmitter.sbr" : \
$(SOURCE) $(DEP_CPP_INSTRU) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_INSTRU=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\InstructionEmitter.obj"	"$(INTDIR)\InstructionEmitter.sbr" : \
$(SOURCE) $(DEP_CPP_INSTRU) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\NativeCodeCache.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_NATIVE=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\StackWalker.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_NATIVE=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\NativeCodeCache.obj"	"$(INTDIR)\NativeCodeCache.sbr" : $(SOURCE)\
 $(DEP_CPP_NATIVE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_NATIVE=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\StackWalker.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\NativeCodeCache.obj"	"$(INTDIR)\NativeCodeCache.sbr" : $(SOURCE)\
 $(DEP_CPP_NATIVE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\NativeFormatter.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_NATIVEF=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_NATIVEF=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\NativeFormatter.obj"	"$(INTDIR)\NativeFormatter.sbr" : $(SOURCE)\
 $(DEP_CPP_NATIVEF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_NATIVEF=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\NativeFormatter.obj"	"$(INTDIR)\NativeFormatter.sbr" : $(SOURCE)\
 $(DEP_CPP_NATIVEF) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\CodeGenerator\Scheduler.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_SCHED=\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\Scheduler.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_SCHED=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Scheduler.obj"	"$(INTDIR)\Scheduler.sbr" : $(SOURCE)\
 $(DEP_CPP_SCHED) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_SCHED=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\Scheduler.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Scheduler.obj"	"$(INTDIR)\Scheduler.sbr" : $(SOURCE)\
 $(DEP_CPP_SCHED) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\RegisterAllocator\Coloring.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_COLOR=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Coloring.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_COLOR=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Coloring.obj"	"$(INTDIR)\Coloring.sbr" : $(SOURCE) $(DEP_CPP_COLOR)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_COLOR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\RegisterAllocator\Coloring.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\Coloring.obj"	"$(INTDIR)\Coloring.sbr" : $(SOURCE) $(DEP_CPP_COLOR)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\RegisterAllocator\RegisterAllocator.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_REGIS=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Coloring.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAllocator.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h"\
	"..\..\..\Compiler\RegisterAllocator\Spilling.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fifo.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_REGIS=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\RegisterAllocator.obj"	"$(INTDIR)\RegisterAllocator.sbr" : $(SOURCE)\
 $(DEP_CPP_REGIS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_REGIS=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Coloring.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAllocator.h"\
	"..\..\..\Compiler\RegisterAllocator\RegisterAssigner.h"\
	"..\..\..\Compiler\RegisterAllocator\Spilling.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fifo.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\RegisterAllocator.obj"	"$(INTDIR)\RegisterAllocator.sbr" : $(SOURCE)\
 $(DEP_CPP_REGIS) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\RegisterAllocator\Spilling.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_SPILL=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Spilling.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fifo.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_SPILL=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Spilling.obj"	"$(INTDIR)\Spilling.sbr" : $(SOURCE) $(DEP_CPP_SPILL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_SPILL=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\Spilling.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\Fifo.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Spilling.obj"	"$(INTDIR)\Spilling.sbr" : $(SOURCE) $(DEP_CPP_SPILL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\RegisterAllocator\VirtualRegister.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_VIRTU=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_VIRTU=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\VirtualRegister.obj"	"$(INTDIR)\VirtualRegister.sbr" : $(SOURCE)\
 $(DEP_CPP_VIRTU) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_VIRTU=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Pool.h"\
	

"$(INTDIR)\VirtualRegister.obj"	"$(INTDIR)\VirtualRegister.sbr" : $(SOURCE)\
 $(DEP_CPP_VIRTU) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\Optimizer\PrimitiveOptimizer.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_PRIMI=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_PRIMI=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\PrimitiveOptimizer.obj"	"$(INTDIR)\PrimitiveOptimizer.sbr" : \
$(SOURCE) $(DEP_CPP_PRIMI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_PRIMI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\PrimitiveOptimizer.obj"	"$(INTDIR)\PrimitiveOptimizer.sbr" : \
$(SOURCE) $(DEP_CPP_PRIMI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\Address.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_ADDRE=\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_ADDRE=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Address.obj"	"$(INTDIR)\Address.sbr" : $(SOURCE) $(DEP_CPP_ADDRE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_ADDRE=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Address.obj"	"$(INTDIR)\Address.sbr" : $(SOURCE) $(DEP_CPP_ADDRE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\ControlGraph.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CONTR=\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\IGVisualizer.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_CONTR=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ControlGraph.obj"	"$(INTDIR)\ControlGraph.sbr" : $(SOURCE)\
 $(DEP_CPP_CONTR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CONTR=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\CodeGenerator.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeFormatter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\ControlGraph.obj"	"$(INTDIR)\ControlGraph.sbr" : $(SOURCE)\
 $(DEP_CPP_CONTR) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\ControlNodes.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_CONTRO=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_CONTRO=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\ControlNodes.obj"	"$(INTDIR)\ControlNodes.sbr" : $(SOURCE)\
 $(DEP_CPP_CONTRO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_CONTRO=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\ControlNodes.obj"	"$(INTDIR)\ControlNodes.sbr" : $(SOURCE)\
 $(DEP_CPP_CONTRO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_PRIMIT=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_PRIMIT=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\PrimitiveBuilders.obj"	"$(INTDIR)\PrimitiveBuilders.sbr" : $(SOURCE)\
 $(DEP_CPP_PRIMIT) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_PRIMIT=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveBuilders.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\PrimitiveBuilders.obj"	"$(INTDIR)\PrimitiveBuilders.sbr" : $(SOURCE)\
 $(DEP_CPP_PRIMIT) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveGraphVerifier.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_PRIMITI=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_PRIMITI=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\PrimitiveGraphVerifier.obj"	"$(INTDIR)\PrimitiveGraphVerifier.sbr" :\
 $(SOURCE) $(DEP_CPP_PRIMITI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_PRIMITI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\GraphUtils.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\PrimitiveGraphVerifier.obj"	"$(INTDIR)\PrimitiveGraphVerifier.sbr" :\
 $(SOURCE) $(DEP_CPP_PRIMITI) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations

!IF  "$(CFG)" == "electricalfire - Win32 Release"

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

InputPath=..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations
InputName=PrimitiveOperations

"genfiles\$(InputName).h"	"genfiles\$(InputName).cpp"	 : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
	$(NSTOOLS)\perl5\perl -I"..\..\..\Tools\PrimitiveOperations"\
                                          ..\..\..\Tools\PrimitiveOperations\MakePrimOp.pl $(InputPath)\
                                          genfiles\$(InputName).h genfiles\$(InputName).cpp

!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\Primitives.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_PRIMITIV=\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\compiler\primitivegraph\primitiveoperations.cpp"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_PRIMITIV=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Primitives.obj"	"$(INTDIR)\Primitives.sbr" : $(SOURCE)\
 $(DEP_CPP_PRIMITIV) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_PRIMITIV=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\compiler\primitivegraph\primitiveoperations.cpp"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\Primitives.obj"	"$(INTDIR)\Primitives.sbr" : $(SOURCE)\
 $(DEP_CPP_PRIMITIV) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\SysCalls.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_SYSCAL=\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_SYSCAL=\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\SysCalls.obj"	"$(INTDIR)\SysCalls.sbr" : $(SOURCE) $(DEP_CPP_SYSCAL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_SYSCAL=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\SysCalls.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\md\x86\x86SysCallsRuntime.h"\
	"..\..\..\Runtime\System\SysCallsRuntime.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\SysCalls.obj"	"$(INTDIR)\SysCalls.sbr" : $(SOURCE) $(DEP_CPP_SYSCAL)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Compiler\PrimitiveGraph\Value.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_VALUE=\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	
NODEP_CPP_VALUE=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Value.obj"	"$(INTDIR)\Value.sbr" : $(SOURCE) $(DEP_CPP_VALUE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_VALUE=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	

"$(INTDIR)\Value.obj"	"$(INTDIR)\Value.sbr" : $(SOURCE) $(DEP_CPP_VALUE)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Debugger\Debugger.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_DEBUGG=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Debugger\jvmdi.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_DEBUGG=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	"..\..\..\Utilities\General\prprf.h"\
	

"$(INTDIR)\Debugger.obj"	"$(INTDIR)\Debugger.sbr" : $(SOURCE) $(DEP_CPP_DEBUGG)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_DEBUGG=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Debugger\jvmdi.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\StringUtils.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\Debugger.obj"	"$(INTDIR)\Debugger.sbr" : $(SOURCE) $(DEP_CPP_DEBUGG)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Debugger\jvmdi.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_JVMDI=\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\InstructionEmitter.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Formatter.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Opcode.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Emitter.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Debugger\jvmdi.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	
NODEP_CPP_JVMDI=\
	"..\..\..\Compiler\CodeGenerator\md\HPPACpu.h"\
	"..\..\..\Compiler\CodeGenerator\md\PPC601Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\prmon.h"\
	"..\..\..\Compiler\CodeGenerator\prtypes.h"\
	"..\..\..\Debugger\Communication\prio.h"\
	"..\..\..\Debugger\Communication\prlock.h"\
	"..\..\..\Debugger\Communication\prprf.h"\
	"..\..\..\Debugger\nspr.h"\
	"..\..\..\Runtime\ClassReader\prtypes.h"\
	"..\..\..\Runtime\FileReader\prio.h"\
	"..\..\..\Runtime\FileReader\prtypes.h"\
	"..\..\..\Runtime\NativeMethods\prlink.h"\
	"..\..\..\Runtime\System\plstr.h"\
	"..\..\..\Runtime\System\pratom.h"\
	"..\..\..\Runtime\System\prcvar.h"\
	"..\..\..\Runtime\System\prlock.h"\
	"..\..\..\Runtime\System\prprf.h"\
	"..\..\..\Runtime\System\prthread.h"\
	"..\..\..\Runtime\System\prtypes.h"\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prbit.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\jvmdi.obj"	"$(INTDIR)\jvmdi.sbr" : $(SOURCE) $(DEP_CPP_JVMDI)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_JVMDI=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\nspr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\plstr.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\pratom.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prbit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prclist.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcvar.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prdtoa.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prenv.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prerror.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinit.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlink.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlock.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prmon.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prnetdb.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prprf.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prproces.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prsystem.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prthread.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Compiler\CodeGenerator\ExceptionTable.h"\
	"..\..\..\Compiler\CodeGenerator\FormatStructures.h"\
	"..\..\..\Compiler\CodeGenerator\Instruction.h"\
	"..\..\..\Compiler\CodeGenerator\md\CpuInfo.h"\
	"..\..\..\Compiler\CodeGenerator\md\x86\x86Win32Cpu.h"\
	"..\..\..\Compiler\CodeGenerator\NativeCodeCache.h"\
	"..\..\..\Compiler\FrontEnd\BytecodeGraph.h"\
	"..\..\..\Compiler\FrontEnd\ErrorHandling.h"\
	"..\..\..\Compiler\FrontEnd\LocalEnv.h"\
	"..\..\..\Compiler\FrontEnd\TranslationEnv.h"\
	"..\..\..\Compiler\FrontEnd\VerificationEnv.h"\
	"..\..\..\Compiler\Optimizer\PrimitiveOptimizer.h"\
	"..\..\..\Compiler\PrimitiveGraph\Address.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlGraph.h"\
	"..\..\..\Compiler\PrimitiveGraph\ControlNodes.h"\
	"..\..\..\Compiler\PrimitiveGraph\PrimitiveOperations.h"\
	"..\..\..\Compiler\PrimitiveGraph\Primitives.h"\
	"..\..\..\Compiler\PrimitiveGraph\Value.h"\
	"..\..\..\Compiler\RegisterAllocator\VirtualRegister.h"\
	"..\..\..\Debugger\Communication\DebuggerChannel.h"\
	"..\..\..\Debugger\Debugger.h"\
	"..\..\..\Debugger\jvmdi.h"\
	"..\..\..\Exports\jni.h"\
	"..\..\..\Exports\md\jni_md.h"\
	"..\..\..\Runtime\ClassInfo\ClassCentral.h"\
	"..\..\..\Runtime\ClassInfo\ClassFileSummary.h"\
	"..\..\..\Runtime\ClassInfo\Collector.h"\
	"..\..\..\Runtime\ClassInfo\PathComponent.h"\
	"..\..\..\Runtime\ClassInfo\StringPool.h"\
	"..\..\..\Runtime\ClassReader\Attributes.h"\
	"..\..\..\Runtime\ClassReader\ClassReader.h"\
	"..\..\..\Runtime\ClassReader\ConstantPool.h"\
	"..\..\..\Runtime\ClassReader\InfoItem.h"\
	"..\..\..\Runtime\ClassReader\utils.h"\
	"..\..\..\Runtime\FileReader\FileReader.h"\
	"..\..\..\Runtime\FileReader\ZipArchive.h"\
	"..\..\..\Runtime\NativeMethods\NameMangler.h"\
	"..\..\..\Runtime\NativeMethods\NativeDefs.h"\
	"..\..\..\Runtime\NativeMethods\NativeMethodDispatcher.h"\
	"..\..\..\Runtime\System\ClassWorld.h"\
	"..\..\..\Runtime\System\FieldOrMethod.h"\
	"..\..\..\Runtime\System\InterfaceDispatchTable.h"\
	"..\..\..\Runtime\System\JavaObject.h"\
	"..\..\..\Runtime\System\JavaString.h"\
	"..\..\..\Runtime\System\JavaVM.h"\
	"..\..\..\Runtime\System\Method.h"\
	"..\..\..\Runtime\System\Monitor.h"\
	"..\..\..\Runtime\System\Thread.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\CheckedUnion.h"\
	"..\..\..\Utilities\General\DebugUtils.h"\
	"..\..\..\Utilities\General\DoublyLinkedList.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\FastBitMatrix.h"\
	"..\..\..\Utilities\General\FastBitSet.h"\
	"..\..\..\Utilities\General\FastHashTable.h"\
	"..\..\..\Utilities\General\FloatUtils.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\HashTable.h"\
	"..\..\..\Utilities\General\InterestingEvents.h"\
	"..\..\..\Utilities\General\JavaBytecodes.h"\
	"..\..\..\Utilities\General\LogModule.h"\
	"..\..\..\Utilities\General\Pool.h"\
	"..\..\..\Utilities\General\Vector.h"\
	"..\..\..\Utilities\zlib\zconf.h"\
	"..\..\..\Utilities\zlib\zlib.h"\
	

"$(INTDIR)\jvmdi.obj"	"$(INTDIR)\jvmdi.sbr" : $(SOURCE) $(DEP_CPP_JVMDI)\
 "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\..\..\Debugger\Win32BreakPoint.cpp

!IF  "$(CFG)" == "electricalfire - Win32 Release"

DEP_CPP_WIN32B=\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	"..\..\..\Utilities\General\Nonspr.h"\
	
NODEP_CPP_WIN32B=\
	"..\..\..\Utilities\General\plstr.h"\
	"..\..\..\Utilities\General\prlog.h"\
	"..\..\..\Utilities\General\prlong.h"\
	

"$(INTDIR)\Win32BreakPoint.obj"	"$(INTDIR)\Win32BreakPoint.sbr" : $(SOURCE)\
 $(DEP_CPP_WIN32B) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

DEP_CPP_WIN32B=\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\obsolete\protypes.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prcpucfg.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinet.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prinrval.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prio.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlog.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prlong.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtime.h"\
	"..\..\..\..\dist\WINNT4.0_x86_DBG.OBJ\include\prtypes.h"\
	"..\..\..\Utilities\General\CatchAssert.h"\
	"..\..\..\Utilities\General\Exports.h"\
	"..\..\..\Utilities\General\Fundamentals.h"\
	

"$(INTDIR)\Win32BreakPoint.obj"	"$(INTDIR)\Win32BreakPoint.sbr" : $(SOURCE)\
 $(DEP_CPP_WIN32B) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

!IF  "$(CFG)" == "electricalfire - Win32 Release"

"Burg - Win32 Release" : 
   cd ".\Burg"
   $(MAKE) /$(MAKEFLAGS) /F .\Burg.mak CFG="Burg - Win32 Release" 
   cd ".."

"Burg - Win32 ReleaseCLEAN" : 
   cd ".\Burg"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\Burg.mak CFG="Burg - Win32 Release"\
 RECURSE=1 
   cd ".."

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

"Burg - Win32 Debug" : 
   cd ".\Burg"
   $(MAKE) /$(MAKEFLAGS) /F .\Burg.mak CFG="Burg - Win32 Debug" 
   cd ".."

"Burg - Win32 DebugCLEAN" : 
   cd ".\Burg"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\Burg.mak CFG="Burg - Win32 Debug" RECURSE=1\
 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "electricalfire - Win32 Release"

"DebuggerChannel - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\DebuggerChannel.mak\
 CFG="DebuggerChannel - Win32 Release" 
   cd "."

"DebuggerChannel - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\DebuggerChannel.mak\
 CFG="DebuggerChannel - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "electricalfire - Win32 Debug"

"DebuggerChannel - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\DebuggerChannel.mak\
 CFG="DebuggerChannel - Win32 Debug" 
   cd "."

"DebuggerChannel - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\DebuggerChannel.mak\
 CFG="DebuggerChannel - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 


!ENDIF 

