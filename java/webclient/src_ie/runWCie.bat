: Script to run webclient in development environment...

cd D:\Projects\mozilla\MOZILLA_1_3\mozilla\java

set CLASSPATH=D:\Projects\mozilla\MOZILLA_1_3\mozilla\dist\classes;%CLASSPATH%

set WCIEPATH=D:\Projects\mozilla\MOZILLA_1_3\mozilla\java\webclient\src_ie

set PATH=%WCIEPATH%;%PATH%

java -Djava.library.path=%WCIEPATH% -classpath %CLASSPATH% org.mozilla.webclient.test.EmbeddedMozillaImpl %WCIEPATH%


