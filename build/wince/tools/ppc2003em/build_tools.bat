rm arm-wince-as.exe
rm arm-wince-gcc.exe
rm arm-wince-lib.exe
rm arm-wince-link.exe

rm *.obj
rm *.ilk
rm *.pdb

cl /O2 arm-wince-as.c 
cl /O2 arm-wince-gcc.c
cl /O2 arm-wince-lib.c
cl /O2 arm-wince-link.c

rm *.obj
rm *.ilk
rm *.pdb