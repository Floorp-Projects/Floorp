SET PATH=%MOZHOME%\ie;%JDKHOME%\jre\bin;%PATH%



%JDKHOME%\bin\java -Djava.library.path=%MOZHOME%\ie -classpath webclient_1_3_win32.jar org.mozilla.webclient.test.EmbeddedMozillaImpl %MOZHOME%\ie
