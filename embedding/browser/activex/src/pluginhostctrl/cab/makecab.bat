@echo off
set CABARCEXE=d:\m\cabsdk\bin\cabarc.exe
set SIGNCODEXE=

set OUTDIR=.\out

set PLUGINHOSTCTRLDLL=%1
set CERTFILE=%2
set KEYFILE=%3

if EXIST %CABARCEXE% goto got_cabarc
echo ERROR: The CABARC tool is not at %CABARCEXE%, so can't proceed.
goto end
:got_cabarc

if NOT .%PLUGINHOSTCTRLDLL%.==.. goto have_pluginhostctrl
echo Usage : %0 pluginhostctrl [certfile keyfile]
echo Specify the path to the pluginhostctrl.dll file as the first argument
echo and optionally the certificate and keyfile as extra arguments.
goto end
:have_pluginhostctrl
if EXIST %PLUGINHOSTCTRLDLL% goto valid_pluginhostctrl
echo ERROR: The specified pluginhostctrl.dll file "%PLUGINHOSTCTRLDLL%" does not exist.
goto end
:valid_pluginhostctrl

mkdir %OUTDIR%
copy %PLUGINHOSTCTRLDLL% %OUTDIR%\pluginhostctrl.dll
copy redist\*.* %OUTDIR%
copy pluginhostctrl.inf %OUTDIR%
%CABARCEXE% -s 6144 -r -P out\ N pluginhostctrl.cab out\*.*

if .%CERTFILE%.==.. goto the_end
if .%KEYFILE%.==.. goto the_end
if NOT EXIST %CERTFILE% goto the_end
if NOT EXIST %KEYFILE$ goto the_end
if NOT EXIST %SIGNCODEEXE% goto the_end

REM TODO signing stuff here

:the_end
set CERTFILE=
set KEYFILE=
set CABARCEXE=
set SIGNCODEEXE=