<html>
<body>
<pre>
<h1>Build Log</h1>
<h3>
--------------------Configuration: shunt - Win32 (WCE ARMV4) Release--------------------
</h3>
<h3>Command Lines</h3>
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP7BE.tmp" with contents
[
/nologo /W3 /I "../include" /D "ARM" /D "_ARM_" /D "ARMV4" /D "NDEBUG" /D _WIN32_WCE=420 /D "WIN32_PLATFORM_PSPC=400" /D UNDER_CE=420 /D "UNICODE" /D "_UNICODE" /D "SHUNT_EXPORTS" /D "MOZCE_SHUNT_EXPORTS" /FR"ARMV4Rel/" /Fo"ARMV4Rel/" /O2 /MC /c 
"C:\builds\wince_port\wince\shunt\win32A.cpp"
]
Creating command line "clarm.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP7BE.tmp" 
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP7BF.tmp" with contents
[
commctrl.lib coredll.lib /nologo /base:"0x00100000" /stack:0x10000,0x1000 /entry:"_DllMainCRTStartup" /dll /pdb:none /incremental:no /nodefaultlib:"libc.lib /nodefaultlib:libcd.lib /nodefaultlib:libcmt.lib /nodefaultlib:libcmtd.lib /nodefaultlib:msvcrt.lib /nodefaultlib:msvcrtd.lib" /out:"ARMV4Rel/shunt.dll" /implib:"ARMV4Rel/shunt.lib" /subsystem:windowsce,4.20 /align:"4096" /MACHINE:ARM 
.\ARMV4Rel\a2w.obj
.\ARMV4Rel\assert.obj
.\ARMV4Rel\direct.obj
.\ARMV4Rel\errno.obj
.\ARMV4Rel\io.obj
.\ARMV4Rel\mbstring.obj
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
Creating command line "link.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP7BF.tmp"
<h3>Output Window</h3>
Compiling...
win32A.cpp
Linking...
   Creating library ARMV4Rel/shunt.lib and object ARMV4Rel/shunt.exp
Creating temporary file "C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP7C3.tmp" with contents
[
/nologo /o"ARMV4Rel/shunt.bsc" 
.\ARMV4Rel\a2w.sbr
.\ARMV4Rel\assert.sbr
.\ARMV4Rel\direct.sbr
.\ARMV4Rel\errno.sbr
.\ARMV4Rel\io.sbr
.\ARMV4Rel\mbstring.sbr
.\ARMV4Rel\process.sbr
.\ARMV4Rel\signal.sbr
.\ARMV4Rel\stat.sbr
.\ARMV4Rel\stdio.sbr
.\ARMV4Rel\stdlib.sbr
.\ARMV4Rel\string.sbr
.\ARMV4Rel\time.sbr
.\ARMV4Rel\w2a.sbr
.\ARMV4Rel\win32.sbr
.\ARMV4Rel\win32A.sbr
.\ARMV4Rel\win32W.sbr]
Creating command line "bscmake.exe @C:\DOCUME~1\dougt\LOCALS~1\Temp\RSP7C3.tmp"
Creating browse info file...
<h3>Output Window</h3>




<h3>Results</h3>
shunt.dll - 0 error(s), 0 warning(s)
</pre>
</body>
</html>
