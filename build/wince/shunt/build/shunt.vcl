<html>
<body>
<pre>
<h1>Build Log</h1>
<h3>
--------------------Configuration: shunt - Win32 (WCE emulator) Release--------------------
</h3>
<h3>Command Lines</h3>
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3362.tmp" with contents
[
/nologo /W3 /I "../include" /D "_i386_" /D "_X86_" /D "x86" /D "NDEBUG" /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D UNDER_CE=420 /D "UNICODE" /D "_UNICODE" /D "SHUNT_EXPORTS" /D "MOZCE_SHUNT_EXPORTS" /Fo"emulatorRel/" /Gs8192 /GF /O2 /c 
"C:\builds\wince\mozilla\build\wince\shunt\stdio.cpp"
]
Creating command line "cl.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3362.tmp" 
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3363.tmp" with contents
[
commctrl.lib coredll.lib corelibc.lib /nologo /base:"0x00100000" /stack:0x5000000 /entry:"_DllMainCRTStartup" /dll /pdb:none /incremental:no /nodefaultlib:"OLDNAMES.lib" /nodefaultlib:libc.lib /nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib /nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib /nodefaultlib:msvcrtd.lib /out:"emulatorRel/shunt.dll" /implib:"emulatorRel/shunt.lib" /subsystem:windowsce,4.20 /MACHINE:IX86 
.\emulatorRel\a2w.obj
.\emulatorRel\assert.obj
.\emulatorRel\direct.obj
.\emulatorRel\errno.obj
.\emulatorRel\io.obj
.\emulatorRel\mbstring.obj
.\emulatorRel\nclog.obj
.\emulatorRel\process.obj
.\emulatorRel\signal.obj
.\emulatorRel\stat.obj
.\emulatorRel\stdio.obj
.\emulatorRel\stdlib.obj
.\emulatorRel\string.obj
.\emulatorRel\time.obj
.\emulatorRel\w2a.obj
.\emulatorRel\win32.obj
.\emulatorRel\win32A.obj
.\emulatorRel\win32W.obj
]
Creating command line "link.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3363.tmp"
<h3>Output Window</h3>
Compiling...
stdio.cpp
Linking...
   Creating library emulatorRel/shunt.lib and object emulatorRel/shunt.exp





<h3>Results</h3>
shunt.dll - 0 error(s), 0 warning(s)
<h3>
--------------------Configuration: shunt - Win32 (WCE emulator) Debug--------------------
</h3>
<h3>Command Lines</h3>
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3367.tmp" with contents
[
/nologo /W3 /Zi /Od /I "../include" /D "DEBUG" /D "_i386_" /D "_X86_" /D "x86" /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D UNDER_CE=420 /D "UNICODE" /D "_UNICODE" /D "SHUNT_EXPORTS" /D "MOZCE_SHUNT_EXPORTS" /Fo"emulatorDbg/" /Fd"emulatorDbg/" /Gs8192 /GF /c 
"C:\builds\wince\mozilla\build\wince\shunt\stdio.cpp"
]
Creating command line "cl.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3367.tmp" 
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3368.tmp" with contents
[
commctrl.lib coredll.lib corelibc.lib /nologo /base:"0x00100000" /stack:0x5000000 /entry:"_DllMainCRTStartup" /dll /pdb:none /incremental:yes /debug /nodefaultlib:"OLDNAMES.lib" /nodefaultlib:libc.lib /nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib /nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib /nodefaultlib:msvcrtd.lib /out:"emulatorDbg/shunt.dll" /implib:"emulatorDbg/shunt.lib" /subsystem:windowsce,4.20 /MACHINE:IX86 
.\emulatorDbg\a2w.obj
.\emulatorDbg\assert.obj
.\emulatorDbg\direct.obj
.\emulatorDbg\errno.obj
.\emulatorDbg\io.obj
.\emulatorDbg\mbstring.obj
.\emulatorDbg\nclog.obj
.\emulatorDbg\process.obj
.\emulatorDbg\signal.obj
.\emulatorDbg\stat.obj
.\emulatorDbg\stdio.obj
.\emulatorDbg\stdlib.obj
.\emulatorDbg\string.obj
.\emulatorDbg\time.obj
.\emulatorDbg\w2a.obj
.\emulatorDbg\win32.obj
.\emulatorDbg\win32A.obj
.\emulatorDbg\win32W.obj
]
Creating command line "link.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3368.tmp"
<h3>Output Window</h3>
Compiling...
stdio.cpp
Linking...
LINK : warning LNK4075: ignoring '/INCREMENTAL' due to '/PDB:NONE' specification
   Creating library emulatorDbg/shunt.lib and object emulatorDbg/shunt.exp





<h3>Results</h3>
shunt.dll - 0 error(s), 1 warning(s)
<h3>
--------------------Configuration: shunt - Win32 (WCE ARMV4) Release--------------------
</h3>
<h3>Command Lines</h3>
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP336C.tmp" with contents
[
/nologo /W3 /I "../include" /D "ARM" /D "_ARM_" /D "ARMV4" /D "NDEBUG" /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D UNDER_CE=420 /D "UNICODE" /D "_UNICODE" /D "SHUNT_EXPORTS" /D "MOZCE_SHUNT_EXPORTS" /FR"ARMV4Rel/" /Fo"ARMV4Rel/" /O2 /MC /c 
"C:\builds\wince\mozilla\build\wince\shunt\stdio.cpp"
]
Creating command line "clarm.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP336C.tmp" 
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP336D.tmp" with contents
[
commctrl.lib coredll.lib /nologo /base:"0x00100000" /stack:0x5000000 /entry:"_DllMainCRTStartup" /dll /pdb:none /incremental:no /nodefaultlib:"libc.lib /nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib /nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib /nodefaultlib:msvcrtd.lib" /out:"ARMV4Rel/shunt.dll" /implib:"ARMV4Rel/shunt.lib" /subsystem:windowsce,4.20 /align:"4096" /MACHINE:ARM 
.\ARMV4Rel\a2w.obj
.\ARMV4Rel\assert.obj
.\ARMV4Rel\direct.obj
.\ARMV4Rel\errno.obj
.\ARMV4Rel\io.obj
.\ARMV4Rel\mbstring.obj
.\ARMV4Rel\nclog.obj
.\ARMV4Rel\process.obj
.\ARMV4Rel\signal.obj
.\ARMV4Rel\stat.obj
.\ARMV4Rel\stdio.obj
.\ARMV4Rel\stdlib.obj
.\ARMV4Rel\string.obj
.\ARMV4Rel\time.obj
.\ARMV4Rel\w2a.obj
.\ARMV4Rel\win32.obj
.\ARMV4Rel\win32A.obj
.\ARMV4Rel\win32W.obj
]
Creating command line "link.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP336D.tmp"
<h3>Output Window</h3>
Compiling...
stdio.cpp
Linking...
   Creating library ARMV4Rel/shunt.lib and object ARMV4Rel/shunt.exp





<h3>Results</h3>
shunt.dll - 0 error(s), 0 warning(s)
<h3>
--------------------Configuration: shunt - Win32 (WCE ARMV4) Debug--------------------
</h3>
<h3>Command Lines</h3>
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3371.tmp" with contents
[
/nologo /W3 /Zi /Od /I "../include" /D "DEBUG" /D "ARM" /D "_ARM_" /D "ARMV4" /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D UNDER_CE=420 /D "UNICODE" /D "_UNICODE" /D "SHUNT_EXPORTS" /D "MOZCE_SHUNT_EXPORTS" /Fo"ARMV4Dbg/" /Fd"ARMV4Dbg/" /MC /c 
"C:\builds\wince\mozilla\build\wince\shunt\stdio.cpp"
]
Creating command line "clarm.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3371.tmp" 
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3372.tmp" with contents
[
commctrl.lib coredll.lib /nologo /base:"0x00100000" /stack:0x5000000 /entry:"_DllMainCRTStartup" /dll /pdb:none /incremental:yes /debug /nodefaultlib:"libc.lib /nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib /nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib /nodefaultlib:msvcrtd.lib" /out:"ARMV4Dbg/shunt.dll" /implib:"ARMV4Dbg/shunt.lib" /subsystem:windowsce,4.20 /align:"4096" /MACHINE:ARM 
.\ARMV4Dbg\a2w.obj
.\ARMV4Dbg\assert.obj
.\ARMV4Dbg\direct.obj
.\ARMV4Dbg\errno.obj
.\ARMV4Dbg\io.obj
.\ARMV4Dbg\mbstring.obj
.\ARMV4Dbg\nclog.obj
.\ARMV4Dbg\process.obj
.\ARMV4Dbg\signal.obj
.\ARMV4Dbg\stat.obj
.\ARMV4Dbg\stdio.obj
.\ARMV4Dbg\stdlib.obj
.\ARMV4Dbg\string.obj
.\ARMV4Dbg\time.obj
.\ARMV4Dbg\w2a.obj
.\ARMV4Dbg\win32.obj
.\ARMV4Dbg\win32A.obj
.\ARMV4Dbg\win32W.obj
]
Creating command line "link.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3372.tmp"
<h3>Output Window</h3>
Compiling...
stdio.cpp
Linking...
LINK : warning LNK4075: ignoring '/INCREMENTAL' due to '/PDB:NONE' specification
   Creating library ARMV4Dbg/shunt.lib and object ARMV4Dbg/shunt.exp





<h3>Results</h3>
shunt.dll - 0 error(s), 1 warning(s)
<h3>
--------------------Configuration: shunt - Win32 (WCE ARMV4) SmartPhone--------------------
</h3>
<h3>Command Lines</h3>
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3376.tmp" with contents
[
/nologo /W3 /I "../include" /D "ARM" /D "_ARM_" /D "ARMV4" /D "NDEBUG" /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D UNDER_CE=420 /D "UNICODE" /D "_UNICODE" /D "SHUNT_EXPORTS" /D "MOZCE_SHUNT_EXPORTS" /FR"ARMV4SmartPhone/" /Fo"ARMV4SmartPhone/" /O2 /MC /c 
"C:\builds\wince\mozilla\build\wince\shunt\stdio.cpp"
]
Creating command line "clarm.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3376.tmp" 
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3377.tmp" with contents
[
commctrl.lib coredll.lib /nologo /base:"0x00100000" /stack:0x5000000 /entry:"_DllMainCRTStartup" /dll /pdb:none /incremental:no /nodefaultlib:"libc.lib /nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib /nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib /nodefaultlib:msvcrtd.lib" /out:"ARMV4SmartPhone/shunt.dll" /implib:"ARMV4SmartPhone/shunt.lib" /subsystem:windowsce,4.20 /align:"4096" /MACHINE:ARM 
.\ARMV4SmartPhone\a2w.obj
.\ARMV4SmartPhone\assert.obj
.\ARMV4SmartPhone\direct.obj
.\ARMV4SmartPhone\errno.obj
.\ARMV4SmartPhone\io.obj
.\ARMV4SmartPhone\mbstring.obj
.\ARMV4SmartPhone\nclog.obj
.\ARMV4SmartPhone\process.obj
.\ARMV4SmartPhone\signal.obj
.\ARMV4SmartPhone\stat.obj
.\ARMV4SmartPhone\stdio.obj
.\ARMV4SmartPhone\stdlib.obj
.\ARMV4SmartPhone\string.obj
.\ARMV4SmartPhone\time.obj
.\ARMV4SmartPhone\w2a.obj
.\ARMV4SmartPhone\win32.obj
.\ARMV4SmartPhone\win32A.obj
.\ARMV4SmartPhone\win32W.obj
]
Creating command line "link.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP3377.tmp"
<h3>Output Window</h3>
Compiling...
stdio.cpp
Linking...
   Creating library ARMV4SmartPhone/shunt.lib and object ARMV4SmartPhone/shunt.exp





<h3>Results</h3>
shunt.dll - 0 error(s), 0 warning(s)
</pre>
</body>
</html>
