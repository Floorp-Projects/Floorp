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

echo Testing simple html to html ...
TestOutput -i text/html -o text/html -f 0 -c OutTestData/simple.html OutTestData/simple.html
IF ERRORLEVEL 1 echo Simple html to html failed (%errorlevel%). && set errmsg=%errmsg% simple.html

echo Testing simple copy case ...
TestOutput -i text/html -o text/plain -f 0 -w 0 -c OutTestData/simplecopy.out OutTestData/simple.html
IF ERRORLEVEL 1 echo Simple copy test failed. && set errmsg=%errmsg% simplecopy.out

echo Testing non-wrapped plaintext ...
TestOutput -i text/html -o text/plain -f 0 -w 0 -c OutTestData/plainnowrap.out OutTestData/plain.html
IF ERRORLEVEL 1 echo Non-wrapped plaintext test failed. && set errmsg=%errmsg% plainnowrap.out

REM echo Testing wrapped bug unformatted plaintext ...
REM TestOutput -i text/html -o text/plain -f 32 -w 50 -c OutTestData/plainwrap.out OutTestData/plain.html
REM IF ERRORLEVEL 1 echo Wrapped plaintext test failed. && set errmsg=%errmsg% plainwrap.out

echo Testing mail quoting ...
TestOutput -i text/html -o text/plain -c OutTestData/mailquote.out OutTestData/mailquote.html
IF ERRORLEVEL 1 echo Mail quoting test failed. && set errmsg=%errmsg% mailquote.out

echo Testing conversion of XIF entities ...
TestOutput -i text/xif -o text/plain -c OutTestData/entityxif.out OutTestData/entityxif.xif
IF ERRORLEVEL 1 echo XIF entity conversion test failed. && set errmsg=%errmsg% entityxif.out

echo Testing XIF to HTML ...
TestOutput -i text/xif -o text/html -c OutTestData/xifstuff.out OutTestData/xifstuff.xif
IF ERRORLEVEL 1 echo XIF to HTML conversion test failed. && set errmsg=%errmsg% xifstuff.out

echo Testing HTML Table to Text ...
TestOutput -i text/html -o text/plain -c OutTestData/htmltable.out OutTestData/htmltable.html
IF ERRORLEVEL 1 echo HTML Table to Plain text failed (%errorlevel%). && set errmsg=%errmsg% htmltable.out

IF DEFINED %errmsg% echo  && echo TESTS FAILED: %errmsg% && exit 1
echo ALL TESTS SUCCEEDED
