@echo off
set LIB=..\xmlparse\Release;..\lib;%LIB%
cl /nologo /DXMLTOKAPI=__declspec(dllimport) /DXMLPARSEAPI=__declspec(dllimport) /I..\xmlparse /Fe..\bin\elements elements.c xmlparse.lib
@echo Run it using: ..\bin\elements ^<..\expat.html
