@echo off

cd table
call rtest.bat %1
cd..

cd images
call rtest.bat %1
cd..

cd general
call rtest.bat %1
cd..
