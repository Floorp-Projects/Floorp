rem * set JDKHOME to your jdk directory
set JDKHOME=c:\java\jdk1.3

rem * set DIST to your mozilla binaries
set DIST=c:\ws\mozilla\mozilla\dist\win32_d.obj

set PATH=%path%;%jdkhome%\jre\bin;%jdkhome%\jre\bin\classic;%dist%\bin\components
set CLASSPATH=%claspath%;%dist%\..\classes;.