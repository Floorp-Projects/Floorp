@echo off
REM  PERL issues 'system' calls to a different session with each 'system'
REM  command, the commands below must happen within the same "session".
REM 

REM Set the BuildType
if "%MOZ_DEBUG%"=="1" set BuildType=debug
if "%MOZ_DEBUG%"=="0" set BuildType=release

REM Set the environment vars.
@echo Setting System Vars.
call C:\"Program Files"\"Microsoft Visual Studio"\VC98\Bin\vcvars32.bat

REM build the damn thing, then send notification if the exe is there.
@echo Building Wizardmachine.
if "%MOZ_DEBUG%"=="1" NMAKE /f "WizardMachine.mak" CFG="WizardMachine - Win32 Debug"
if "%MOZ_DEBUG%"=="0" NMAKE /f "WizardMachine.mak" CFG="WizardMachine - Win32 Release"

