rem Copyright (c) 1998, 1999 Thai Open Source Software Center Ltd
rem See the file COPYING for copying permission.

@echo off
set LIB=..\xmlparse\Release;..\lib;%LIB%
cl /nologo /DXMLTOKAPI=__declspec(dllimport) /DXMLPARSEAPI=__declspec(dllimport) /I..\xmlparse /Fe..\bin\elements elements.c xmlparse.lib
@echo Run it using: ..\bin\elements ^<..\expat.html
