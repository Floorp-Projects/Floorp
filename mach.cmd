@ECHO OFF
SET topsrcdir=%~dp0

WHERE /q py
IF %ERRORLEVEL% EQU 0 (
  py %topsrcdir%mach %*
) ELSE (
  python %topsrcdir%mach %*
)
