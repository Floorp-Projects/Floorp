@echo off

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

