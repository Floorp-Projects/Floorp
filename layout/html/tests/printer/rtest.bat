@echo off

cd general
call rtest.bat %1
cd..

cd table
call rtest.bat %1
cd..

rem cd images
rem call rtest.bat %1
rem cd..

