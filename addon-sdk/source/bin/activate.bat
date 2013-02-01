@echo off
rem This Source Code Form is subject to the terms of the Mozilla Public
rem License, v. 2.0. If a copy of the MPL was not distributed with this
rem file, You can obtain one at http://mozilla.org/MPL/2.0/.

set VIRTUAL_ENV=%~dp0
set VIRTUAL_ENV=%VIRTUAL_ENV:~0,-5%
set CUDDLEFISH_ROOT=%VIRTUAL_ENV%

SET PYTHONKEY=SOFTWARE\Python\PythonCore

rem look for 32-bit windows and python, or 64-bit windows and python

SET PYTHONVERSION=2.7
call:CheckPython PYTHONINSTALL %PYTHONKEY%\%PYTHONVERSION%\InstallPath
if "%PYTHONINSTALL%" NEQ "" goto FoundPython

SET PYTHONVERSION=2.6
call:CheckPython PYTHONINSTALL %PYTHONKEY%\%PYTHONVERSION%\InstallPath
if "%PYTHONINSTALL%" NEQ "" goto FoundPython

SET PYTHONVERSION=2.5
call:CheckPython PYTHONINSTALL %PYTHONKEY%\%PYTHONVERSION%\InstallPath
if "%PYTHONINSTALL%" NEQ "" goto FoundPython

if not defined ProgramFiles(x86) goto win32

rem look for 32-bit python on 64-bit windows

SET PYTHONKEY=SOFTWARE\Wow6432Node\Python\PythonCore

SET PYTHONVERSION=2.7
call:CheckPython PYTHONINSTALL %PYTHONKEY%\%PYTHONVERSION%\InstallPath
if "%PYTHONINSTALL%" NEQ "" goto FoundPython

SET PYTHONVERSION=2.6
call:CheckPython PYTHONINSTALL %PYTHONKEY%\%PYTHONVERSION%\InstallPath
if "%PYTHONINSTALL%" NEQ "" goto FoundPython

SET PYTHONVERSION=2.5
call:CheckPython PYTHONINSTALL %PYTHONKEY%\%PYTHONVERSION%\InstallPath
if "%PYTHONINSTALL%" NEQ "" goto FoundPython

:win32

SET PYTHONVERSION=
set PYTHONKEY=
echo Warning: Failed to find Python installation directory
goto :EOF

:FoundPython

if defined _OLD_PYTHONPATH (
    set PYTHONPATH=%_OLD_PYTHONPATH%
)
if not defined PYTHONPATH (
    set PYTHONPATH=;
)
set _OLD_PYTHONPATH=%PYTHONPATH%
set PYTHONPATH=%VIRTUAL_ENV%\python-lib;%PYTHONPATH%

if not defined PROMPT (
    set PROMPT=$P$G
)

if defined _OLD_VIRTUAL_PROMPT (
    set PROMPT=%_OLD_VIRTUAL_PROMPT%
)

set _OLD_VIRTUAL_PROMPT=%PROMPT%
set PROMPT=(%VIRTUAL_ENV%) %PROMPT%

if defined _OLD_VIRTUAL_PATH goto OLDPATH
goto SKIPPATH
:OLDPATH
PATH %_OLD_VIRTUAL_PATH%

:SKIPPATH
set _OLD_VIRTUAL_PATH=%PATH%
PATH %VIRTUAL_ENV%\bin;%PYTHONINSTALL%;%PATH%
set PYTHONKEY=
set PYTHONINSTALL=
set PYTHONVERSION=
set key=
set reg=
set _tokens=
python -c "from jetpack_sdk_env import welcome; welcome()"
GOTO :EOF

:CheckPython
::CheckPython(retVal, key)
::Reads the registry at %2% and checks if a Python exists there.
::Checks both HKLM and HKCU, then checks the executable actually exists.
SET key=%2%
SET "%~1="
SET reg=reg
if defined ProgramFiles(x86) (
  rem 32-bit cmd on 64-bit windows
  if exist %WINDIR%\sysnative\reg.exe SET reg=%WINDIR%\sysnative\reg.exe
)
rem On Vista+, the last line of output is:
rem    (default)  REG_SZ  the_value
rem (but note the word "default" will be localized.
rem On XP, the last line of output is:
rem   <NO NAME>\tREG_SZ\tthe_value
rem (not sure if "NO NAME" is localized or not!)
rem SO: we use ")>" as the tokens to split on, then nuke
rem the REG_SZ and any tabs or spaces.
FOR /F "usebackq tokens=2 delims=)>" %%A IN (`%reg% QUERY HKLM\%key% /ve 2^>NUL`) DO SET "%~1=%%A"
rem Remove the REG_SZ
set PYTHONINSTALL=%PYTHONINSTALL:REG_SZ=%
rem Remove tabs (note the literal \t in the next line
set PYTHONINSTALL=%PYTHONINSTALL:	=%
rem Remove spaces.
set PYTHONINSTALL=%PYTHONINSTALL: =%
if exist %PYTHONINSTALL%\python.exe goto :EOF
rem It may be a 32bit Python directory built from source, in which case the
rem executable is in the PCBuild directory.
if exist %PYTHONINSTALL%\PCBuild\python.exe (set "PYTHONINSTALL=%PYTHONINSTALL%\PCBuild" & goto :EOF)
rem Or maybe a 64bit build directory.
if exist %PYTHONINSTALL%\PCBuild\amd64\python.exe (set "PYTHONINSTALL=%PYTHONINSTALL%\PCBuild\amd64" & goto :EOF)

rem And try HKCU
FOR /F "usebackq tokens=2 delims=)>" %%A IN (`%reg% QUERY HKCU\%key% /ve 2^>NUL`) DO SET "%~1=%%A"
set PYTHONINSTALL=%PYTHONINSTALL:REG_SZ=%
set PYTHONINSTALL=%PYTHONINSTALL:	=%
set PYTHONINSTALL=%PYTHONINSTALL: =%
if exist %PYTHONINSTALL%\python.exe goto :EOF
if exist %PYTHONINSTALL%\PCBuild\python.exe (set "PYTHONINSTALL=%PYTHONINSTALL%\PCBuild" & goto :EOF)
if exist %PYTHONINSTALL%\PCBuild\amd64\python.exe (set "PYTHONINSTALL=%PYTHONINSTALL%\PCBuild\amd64" & goto :EOF)
rem can't find it here, so arrange to try the next key
set PYTHONINSTALL=

GOTO :EOF
