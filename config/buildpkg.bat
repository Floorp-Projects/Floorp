@echo off
rem The contents of this file are subject to the Netscape Public
rem License Version 1.1 (the "License"); you may not use this file
rem except in compliance with the License. You may obtain a copy of
rem the License at http://www.mozilla.org/NPL/
rem
rem Software distributed under the License is distributed on an "AS
rem IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
rem implied. See the License for the specific language governing
rem rights and limitations under the License.
rem
rem The Original Code is mozilla.org code.
rem
rem The Initial Developer of the Original Code is Netscape
rem Communications Corporation.  Portions created by Netscape are
rem Copyright (C) 1998 Netscape Communications Corporation. All
rem Rights Reserved.
rem
rem Contributor(s): 
@echo on

@echo off
if not exist %2\NUL echo Warning: %2 does not exist! (you may need to check it out)
if not exist %2\NUL exit 1

cd %2

goto NO_CAFE

if "%MOZ_CAFE%"=="" goto NO_CAFE

mkdir %MOZ_SRC%\mozilla\dist\classes\%2
%MOZ_TOOLS%\bin\sj.exe -classpath %MOZ_SRC%\mozilla\dist\classes;%MOZ_SRC%\mozilla\sun-java\classsrc -d %MOZ_SRC%\mozilla\dist\classes *.java 
goto END

:NO_CAFE

perl.exe %MOZ_SRC%\mozilla\config\outofdate.pl -d %MOZ_SRC%\mozilla\dist\classes\%2 -cfg %1 *.java > doit.bat
call doit.bat
del doit.bat

:END

