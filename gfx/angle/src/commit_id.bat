@echo off

REM commit hash
(FOR /F "delims=" %%i IN ('call git rev-parse --short^=12 HEAD') DO set _Str=%%i) || (set _Str=badf00dbad00)
set _Str=#define ANGLE_COMMIT_HASH "%_Str%"
echo %_Str% > %1%\commit.h

REM commit hash size
set _Str=#define ANGLE_COMMIT_HASH_SIZE 12
echo %_Str% >> %1%\commit.h

REM commit date
(FOR /F "delims=" %%i IN ('call git show -s --format^="%%ci" HEAD') DO set _Str=%%i) || (set _Str=Unknown Date)
set _Str=#define ANGLE_COMMIT_DATE "%_Str%"
echo %_Str% >> %1%\commit.h
