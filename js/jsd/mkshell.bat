@echo off
REM nmake -f jsdshell.mak JSDEBUGGER_JAVA_UI=1 LIVECONNECT=1 %1 %2 %3 %4 %5
@echo on
nmake -f jsdshell.mak JSDEBUGGER_JAVA_UI=1 %1 %2 %3 %4 %5
