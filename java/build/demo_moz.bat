set CLASSPATH=%MOZHOME%\webclient_1_3_win32.jar

set PATH=%PATH%;%MOZHOME%;%MOZHOME%\components;%JDKHOME%\jre\bin
%JDKHOME%\bin\java org.mozilla.webclient.test.EmbeddedMozillaImpl %MOZHOME%