@echo off

copy *.js ..\dist\bin\Debug >NUL
cd ..\dist\bin\Debug
jre -classpath ..\..\classes;..\..\classes\ifc11.jar;..\..\..\..\rhino;%CLASSPATH% com.netscape.jsdebugging.jsdb.Main %1 %2 %3 %4 %6 %7 %8
cd ..\..\..\test
