REM rm arm-wince-as.exe
REM rm arm-wince-gcc.exe
REM rm arm-wince-lib.exe
REM rm arm-wince-link.exe

rm *.obj
rm *.ilk
rm *.pdb

cl arm-wince-as.c 
cl arm-wince-gcc.c
cl arm-wince-lib.c
cl arm-wince-link.c 
   
rm *.obj
rm *.ilk
rm *.pdb
