cd 7zstage
7z a -t7z ..\7z\app.7z *.* -m0=LZMA:d25m:fb128 -mx9
cd ..\7z
upx -9 7zSD.sfx
copy /b 7zSD.sfx+app.tag+app.7z SetupGeneric.exe
cd ..
