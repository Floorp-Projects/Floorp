rem @ECHO off
set BLACKWOOD_HOME=%MOZILLA_HOME%
set MOZILLA_FIVE_HOME=%MOZILLA_HOME%\dist\WIN32_D.OBJ\bin
set JAVADOM_HOME=%MOZILLA_HOME%\java\dom
rem Mozilla binary file name
set MOZILLA_BIN=mozilla.exe
set JAVA_HOME=%JAVAHOME%

rem path to the SRC directory of the JavaDOM tests
set TEST_PATH=E:\Mozilla\BW_SQE\Tests_accessor\src\JavaDOM\CoreLevel1\src


rem if this variable is set then we register DOMDocumentListener through TestLoader applet
rem (currently avilable on Win32 only)
rem otherwise hacked DOMAccessorImpl class is used for running tests
set USE_APPLET_FOR_REGISTRATION=1

if %USE_APPLET_FOR_REGISTRATION%x==1x del %TEST_PATH%\..\classes\org\mozilla\dom\DOMAccessorImpl.class 2> nul;
if %USE_APPLET_FOR_REGISTRATION%x==1x del %TEST_PATH%\..\classes\org\mozilla\dom\DocumentImpl.class 2> nul;

rem url of the directory where you placed test.html and test.xml 
rem complete URL looks like that: $TEST_PATH/test.html(xml)
rem if you register DOMDocumentListener through TestLoader applet (by default under Win32) then 
rem url below is unused - it should be set in the BWProperties file 
rem (BW_HTMLTEST and BW_XMLTEST properties)
rem (file:/test.html and file:/test.xml URLs are used by default)
set TEST_URL=file:

set PATH=%JAVA_HOME%\bin;%PATH%
set PATH=%MOZILLA_FIVE_HOME%;%PATH%

set CLASSPATH=%TEST_PATH%\..\classes;%JAVADOM_HOME%\tests\classes;%MOZILLA_FIVE_HOME%\..\classes;%CLASSPATH%

set PLUGLET=%JAVADOM_HOME%\..\tests\classes

rem creating new console window with these variables being set
start

