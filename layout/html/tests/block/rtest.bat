@echo off

cd base
call rtest.bat %1

cd ..\bugs
call rtest.bat %1

cd ..

