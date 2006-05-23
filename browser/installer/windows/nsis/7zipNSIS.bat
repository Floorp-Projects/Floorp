7z a -t7z app.7z setup.exe config "%1/*" -mx -m0=BCJ2 -m1=LZMA:d24 -m2=LZMA:d19 -m3=LZMA:d19  -mb0:1 -mb0s1:2 -mb0s2:3
upx --best 7zSD.sfx
copy /b 7zSD.sfx+app.tag+app.7z SetupGeneric.exe
