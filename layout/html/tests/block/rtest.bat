@echo off

rem Treat assertions as warnings so the tests don't choke waiting on a
rem dialog box

set XPCOM_DEBUG_BREAK_SAVE=%XPCOM_DEBUG_BREAK%
set XPCOM_DEBUG_BREAK=warn

set HAS_SUBST=0
if exist s:\mozilla\layout\html\tests\block\rtest.bat set HAS_SUBST=1
if not exist s:\mozilla\layout\html\tests\block\rtest.bat subst s: %MOZ_SRC%

cd base
call rtest.bat %1

cd ..\bugs
call rtest.bat %1

cd ..\printing
call rtest.bat %1

cd ..\..\table
call rtest.bat %1

cd ..\formctls
call rtest.bat %1
cd ..\block

if %HAS_SUBST%=0 subst s: /D

set XPCOM_DEBUG_BREAK=%XPCOM_DEBUG_BREAK_SAVE%

