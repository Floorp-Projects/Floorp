cd 7zstage
7z a -t7z ..\7z\app.7z *.* -mx -m0=BCJ2 -m1=LZMA:d24 -m2=LZMA:d19 -m3=LZMA:d19  -mb0:1 -mb0s1:2 -mb0s2:3 
cd ..\7z
upx -9 7zSD.sfx
copy /b 7zSD.sfx+app.tag+app.7z SetupGeneric.exe
cd ..
