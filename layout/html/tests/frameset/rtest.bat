@echo off

cd core
call rtest.bat %1

cd ..\bugs
call rtest.bat %1

cd ..\iframe
call rtest.bat %1

cd ..

