@echo off

rem Treat assertions as warnings so the tests don't choke waiting on a
rem dialog box

set XPCOM_DEBUG_BREAK_SAVE=%XPCOM_DEBUG_BREAK%
set XPCOM_DEBUG_BREAK=warn

cd core
call rtest.bat %1

cd ..\viewer_tests
call rtest.bat %1

cd ..\bugs
call rtest.bat %1

cd ..\marvin
call rtest.bat %1

cd ..\other
call rtest.bat %1

cd ..\dom
call rtest.bat %1

cd ..\printing
call rtest.bat %1

cd ..

set XPCOM_DEBUG_BREAK=%XPCOM_DEBUG_BREAK_SAVE%
