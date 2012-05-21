@echo off
REM This Source Code Form is subject to the terms of the Mozilla Public
REM License, v. 2.0. If a copy of the MPL was not distributed with this
REM file, You can obtain one at http://mozilla.org/MPL/2.0/.

REM nmake -f jsdshell.mak JSDEBUGGER_JAVA_UI=1 %1 %2 %3 %4 %5
@echo on
nmake -f jsdshell.mak JSDEBUGGER_JAVA_UI=1 %1 %2 %3 %4 %5
