@echo off
REM
REM This needs to be run from the src dir. It generates sibling dirs and their
REM contents.
REM

set base_package=com.netscape.jsdebugging.remote.corba
set base_packslash=com\netscape\jsdebugging\remote\corba
set jsdj_classes_dir=..\..\..\jsdj\classes
set DELAY=6

rem -------------------------------------------------------------------------
rem -- show settings
echo.
echo commandline: %0 %1 %2 %3 %4 %5 %6 %7 %8 %9
echo.
echo ES3_ROOT           = %ES3_ROOT%
echo base_package       = %base_package% // set in this file
echo jsdj_classes_dir   = %jsdj_classes_dir% // set in this file
echo.
rem -------------------------------------------------------------------------

rem -- check for environment settings
if "%ES3_ROOT%"         == "" goto usage

set jc=sj.exe
set cp=.;%ES3_ROOT%\wai\java\nisb.zip;%ES3_ROOT%\wai\java\WAI.zip;%ES3_ROOT%\plugins\Java\classes\serv3_0.zip
set old_classpath=%CLASSPATH%
set CLASSPATH=%CLASSPATH%;%ES3_ROOT%\wai\java\nisb.zip;%ES3_ROOT%\wai\java\WAI.zip
echo.
echo creating output dirs
if not exist ..\class\NUL mkdir ..\class
if not exist ..\idl\NUL   mkdir ..\idl
if not exist ..\java\NUL  mkdir ..\java
if not exist ..\cpp\NUL   mkdir ..\cpp

echo.
echo compiling raw Java interfaces
%jc% -classpath %cp% *.java -d ..\class


..\src\wait %DELAY%
cd ..\class
echo.
echo.
echo generating idl
echo.
REM
REM THESE ARE HAND ORDERED TO DEAL WITH DEPENDENCIES
REM
%ES3_ROOT%\wai\bin\java2idl Thing.class StringReciever.class TestInterface.class ISourceTextProvider.class IScriptSection.class bogus0.class IScript.class IJSPC.class IJSSourceLocation.class IJSErrorReporter.class IScriptHook.class IJSStackFrameInfo.class bogus1.class IJSThreadState.class IJSExecutionHook.class IExecResult.class IDebugController.class > ..\idl\ifaces.idl


..\src\wait %DELAY%
cd ..\idl
echo.
echo.
echo stripping lines from idl which were added to correctly order declarations
echo.
copy ifaces.idl ifaces_original.idl
REM
REM since we currenly have 2 of these, we can avoid the copy 
REM
gawk -v pat="struct bogus0" -v lines=3 -f ..\src\cutlines.awk < ifaces.idl > temp.idl
gawk -v pat="struct bogus1" -v lines=3 -f ..\src\cutlines.awk < temp.idl > ifaces.idl
REM copy temp.idl ifaces.idl

REM ..\src\wait %DELAY%
cd ..\cpp
echo.
echo.
echo generating cpp
echo.
%ES3_ROOT%\wai\bin\orbeline ..\idl\ifaces.idl


..\src\wait %DELAY%
cd ..\java
echo.
echo.
echo generating java
echo.
%ES3_ROOT%\wai\bin\idl2java ..\idl\ifaces.idl -package %base_package% -no_examples -no_tie -no_comments


..\src\wait %DELAY%
cd ..\src
echo.
echo. copying generated files
echo.
REM
REM preserve generated java, but put ours in the outdir
REM
xcopy /Q ..\java\%base_packslash%\*.java ..\java\%base_packslash%\_unused\*.jav
REM
REM *****CUSTOMIZE HERE AS NEW INTERFACES WITH static ints ARE ADDED*****
REM
copy ..\src\package_header.h+..\src\ISourceTextProvider.java ..\java\%base_packslash%\ISourceTextProvider.java
copy ..\src\package_header.h+..\src\IJSErrorReporter.java ..\java\%base_packslash%\IJSErrorReporter.java
copy ..\src\package_header.h+..\src\IJSThreadState.java ..\java\%base_packslash%\IJSThreadState.java
copy ..\src\package_header.h+..\src\IDebugController.java ..\java\%base_packslash%\IDebugController.java
REM
REM
xcopy /Q ..\cpp\ifaces_c.hh ..\
xcopy /Q ..\cpp\ifaces_s.hh ..\
xcopy /Q ..\cpp\ifaces_c.cc ..\ifaces_c.cpp
xcopy /Q ..\cpp\ifaces_s.cc ..\ifaces_s.cpp

if "%jsdj_classes_dir%" == "" goto done
if not exist %jsdj_classes_dir%\NUL  goto done
xcopy /Q /S ..\java\*.java %jsdj_classes_dir%

goto done

:usage
echo.
echo usage:
echo  mk
echo.
echo See "readme.txt" for details...
echo.
echo Rules:
echo.
echo These must be defined in environment:
echo  ES3_ROOT      // location of Enterprise Server (e.g. e:\Netscape\SuiteSpot)
echo.

:done
..\src\wait %DELAY%
cd ..\src

set base_package=
set base_packslash=
set jsdj_classes_dir=
set cp=
set jc=
set DELAY=
set CLASSPATH=%old_classpath%
set old_classpath=
echo.
echo.
echo done
echo.
