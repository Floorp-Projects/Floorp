cd ..\..\..\..
cvs co mozilla/gc/boehm
cd mozilla\gc\boehm
nmake -f NT_MAKEFILE gc.lib
cd ..\..\js\js2\winbuild
