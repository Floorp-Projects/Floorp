@echo off

if "%1" == "debug" goto debug
if "%1" == "release" goto release

echo Usage: build [target]
echo     Where target is one of the following: debug, release.
if "%1" == "" goto default
goto next

:default
echo No target specified.  Selecting debug as default target. 

:debug
copy ..\..\..\..\dist\win32_d.obj\bin\*.dll data\Program
copy ..\..\..\..\dist\win32_d.obj\bin\trex.exe data\Program
copy ..\..\..\..\dist\win32_d.obj\bin\netscape.cfg data\Program
goto next

:release
copy ..\..\..\..\dist\win32_o.obj\bin\*.dll data\Program
copy ..\..\..\..\dist\win32_o.obj\bin\trex.exe data\Program
copy ..\..\..\..\dist\win32_o.obj\bin\netscape.cfg data\Program


:next
rem ....Build components...
setup\compile setup\setup.rul -g
copy setup\setup.ins disk1
copy setup\setup.dbg disk1
copy ..\..\..\..\trex\modules\parser\calxml\res\julian.cal data\ui
del data\data.z
icomp -i data\*.* data\data.z
copy data\data.z disk1
del data\data.z
cd setup
del setup.pkg
packlist setup.lst
copy setup.pkg ..\disk1
cd ..
copy setup\setup.bmp          disk1
del dist\setupex.exe
rmdir /s /q dist
mkdir dist
start /wait exebuild .\build.ini