@echo off

rem Treat assertions as warnings so the tests don't choke waiting on a
rem dialog box

set XPCOM_DEBUG_BREAK_SAVE=%XPCOM_DEBUG_BREAK%
set XPCOM_DEBUG_BREAK=warn

cd base
call rtest.bat %1

cd ..\bugs
call rtest.bat %1

cd ..

set XPCOM_DEBUG_BREAK=%XPCOM_DEBUG_BREAK_SAVE%
