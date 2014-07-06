@echo off
set _arch=%1
set _arch=%_arch:Win32=x86%
copy %2"\Redist\D3D\"%_arch%"\d3dcompiler_46.dll" %3 > NUL
