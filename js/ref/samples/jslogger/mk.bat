@echo off
set _JSDRUN_=..\..\jsd\run
set _1_=%_JSDRUN_%\classes
set _2_=%_JSDRUN_%\ifc12.jar
set _3_=%_JSDRUN_%\jsd10.jar
set _4_=%_JSDRUN_%\jsdeb15.jar
set _args_=%1 %2 %3 %4 %5 %6 %7 %8 %9
set _files_= *.java netscape\jslogger\*.java

sj -classpath %CLASSPATH%;%_1_%;%_2_%;%_3_%;%_4_%; %_args_% %_files_%

set _JSDRUN_=
set _1_=
set _2_=
set _3_=
set _4_=
set _args_=
set _files_=
