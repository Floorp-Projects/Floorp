@echo off

cd table
call rtest.bat %1

cd images
call rtest.bat %1
cd ..

