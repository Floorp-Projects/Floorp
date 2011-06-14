@echo off

set OLDPATH=%PATH%
set PATH=d:\m\cabsdk\bin;d:\m\codesign\bin;%PATH%

set OUTDIR=.\out
set OUTCAB=.\pluginhostctrl.cab

set PLUGINHOSTCTRLDLL=%1
set CERTFILE=%2
set KEYFILE=%3
set SPCFILE=%4

REM === Check arguments ===

if NOT .%PLUGINHOSTCTRLDLL%.==.. goto have_pluginhostctrl
echo Usage : %0 pluginhostctrl [certfile keyfile spcfile]
echo .
echo Specify the path to the pluginhostctrl.dll file as the first argument
echo and optionally the certificate and keyfile as extra arguments.
goto end
:have_pluginhostctrl
if EXIST %PLUGINHOSTCTRLDLL% goto valid_pluginhostctrl
echo ERROR: The specified pluginhostctrl.dll file "%PLUGINHOSTCTRLDLL%" does not exist.
goto end
:valid_pluginhostctrl


REM === Make the CAB file ===

mkdir %OUTDIR%
copy %PLUGINHOSTCTRLDLL% %OUTDIR%
copy redist\*.* %OUTDIR%
copy pluginhostctrl.inf %OUTDIR%
cabarc -s 6144 -r -P %OUTDIR%\ N %OUTCAB% out\*.*


REM === Generate a test certificate to sign this thing with ===

if NOT .%TESTCERT%.==.. goto end_testcert
echo Generating a test certificate...
set KEYFILE=.\test.key
set CERTFILE=.\test.cer
set SPCFILE=.\test.spc
makecert -sv %KEYFILE% -n "CN=testcert.untrusted.org" %CERTFILE%
cert2spc %CERTFILE% %SPCFILE%
:end_testcert


REM === Sign the CAB file ===

if .%CERTFILE%.==.. goto the_end
if .%KEYFILE%.==.. goto the_end
if .%SPCFILE%.==.. goto the_end
if NOT EXIST %CERTFILE% goto the_end
if NOT EXIST %KEYFILE% goto the_end
if NOT EXIST %SPCFILE% goto the_end
echo Signing %OUTCAB%
signcode -spc %SPCFILE% -v %KEYFILE% -n "Mozilla ActiveX control for hosting plugins" %OUTCAB%


REM == THE END ===

:the_end
set PATH=%OLDPATH%
set OLDPATH=
set OUTCAB=
set CERTFILE=
set KEYFILE=
set SPCFILE=
