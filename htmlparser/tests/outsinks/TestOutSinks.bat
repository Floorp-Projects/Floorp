@echo off
REM The contents of this file are subject to the Netscape Public
REM License Version 1.1 (the "License"); you may not use this file
REM except in compliance with the License. You may obtain a copy of
REM the License at http://www.mozilla.org/NPL/
REM  
REM Software distributed under the License is distributed on an "AS
REM IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
REM implied. See the License for the specific language governing
REM rights and limitations under the License.
REM  
REM The Original Code is Mozilla Communicator client code, released
REM March 31, 1998.
REM 
REM The Initial Developer of the Original Code is Netscape
REM Communications Corporation. Portions created by Netscape are
REM Copyright (C) 1998-1999 Netscape Communications Corporation. All
REM Rights Reserved.
REM 
REM Contributor(s): Akkana Peck, Daniel Bratell.

REM
REM This is a collection of test files to guard against regressions
REM in the Gecko output system.
REM

set errmsg=
set has_err=

echo Testing simple html to html ...
TestOutput -i text/html -o text/html -f 0 -c OutTestData/simple.html OutTestData/simple.html
if errorlevel 1 echo Simple html to html failed (%errorlevel%). && set has_err=1 && set errmsg=%errmsg% simple.html

echo Testing simple copy case ...
TestOutput -i text/html -o text/plain -f 0 -w 0 -c OutTestData/simplecopy.out OutTestData/simple.html
if errorlevel 1 echo Simple copy test failed. && set has_err=1 && set errmsg=%errmsg% simplecopy.out

echo "Testing simple html to plaintext formatting ..."
TestOutput -i text/html -o text/plain -f 34 -w 70 -c OutTestData/simplefmt.out OutTestData/simple.html
if errorlevel 1 echo Simple formatting test failed. && set has_err=1 && set errmsg=%errmsg% plainnowrap.out

echo Testing non-wrapped plaintext ...
TestOutput -i text/html -o text/plain -f 0 -w 0 -c OutTestData/plainnowrap.out OutTestData/plain.html
if errorlevel 1 echo Non-wrapped plaintext test failed. && set has_err=1 && set errmsg=%errmsg% plainnowrap.out

REM echo Testing wrapped bug unformatted plaintext ...
REM TestOutput -i text/html -o text/plain -f 32 -w 50 -c OutTestData/plainwrap.out OutTestData/plain.html
REM if errorlevel 1 echo Wrapped plaintext test failed. && set has_err=1 && set errmsg=%errmsg% plainwrap.out

echo Testing mail quoting ...
TestOutput -i text/html -o text/plain -c OutTestData/mailquote.out OutTestData/mailquote.html
if errorlevel 1 echo Mail quoting test failed. && set has_err=1 && set errmsg=%errmsg% mailquote.out

echo Testing conversion of Xif entities ...
TestOutput -i text/xif -o text/plain -c OutTestData/entityxif.out OutTestData/entityxif.xif
if errorlevel 1 echo Xif entity conversion test failed. && set has_err=1 && set errmsg=%errmsg% entityxif.out

echo Testing Xif to HTML ...
TestOutput -i text/xif -o text/html -c OutTestData/xifstuff.out OutTestData/xifstuff.xif
if errorlevel 1 echo Xif to HTML conversion test failed. && set has_err=1 && set errmsg=%errmsg% xifstuff.out

echo Testing HTML Table to Text ...
TestOutput -i text/html -o text/plain -c OutTestData/htmltable.out OutTestData/htmltable.html
if errorlevel 1 echo HTML Table to Plain text failed (%errorlevel%). && set has_err=1 && set errmsg=%errmsg% htmltable.out

echo "Testing XIF to plain with doctype (bug 28447) ..."
TestOutput -i text/xif -o text/plain -f 2 -c OutTestData/xifdtplain.out OutTestData/doctype.xif
if errorlevel 1 echo XIF to plain with doctype failed (%errorlevel%). && set has_err=1 && set errmsg=%errmsg% xifdtplain.out

REM echo "Testing XIF to html with doctype ..."
REM TestOutput -i text/xif -o text/html -f 0 -c OutTestData/xifdthtml.out OutTestData/doctype.xif
REM if errorlevel 1 echo XIF to html with doctype failed (%errorlevel%). && set has_err=1 && set errmsg=%errmsg% xifdthtml.out

if %has_err% == 0 goto success
echo.
echo TESTS FAILED: %errmsg%
rem exit 1
goto end

:success
echo ALL TESTS SUCCEEDED
:end
