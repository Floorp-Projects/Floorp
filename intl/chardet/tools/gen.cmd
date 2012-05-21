REM This Source Code Form is subject to the terms of the Mozilla Public
REM License, v. 2.0. If a copy of the MPL was not distributed with this
REM file, You can obtain one at http://mozilla.org/MPL/2.0/.

perl gencp1252.pl    > ..\src\nsCP1252Verifier.h
perl geneucjp.pl     > ..\src\nsEUCJPVerifier.h
perl geniso2022jp.pl > ..\src\nsISO2022JPVerifier.h
perl gensjis.pl      > ..\src\nsSJISVerifier.h
perl genutf8.pl      > ..\src\nsUTF8Verifier.h
perl geneuckr.pl     > ..\src\nsEUCKRVerifier.h
perl gengb2312.pl    > ..\src\nsGB2312Verifier.h
perl genbig5.pl      > ..\src\nsBIG5Verifier.h
perl geneuctw.pl     > ..\src\nsEUCTWVerifier.h
perl genucs2be.pl    > ..\src\nsUCS2BEVerifier.h
perl genucs2le.pl    > ..\src\nsUCS2LEVerifier.h
perl genhz.pl        > ..\src\nsHZVerifier.h
perl geniso2022kr.pl > ..\src\nsISO2022KRVerifier.h
perl geniso2022cn.pl > ..\src\nsISO2022CNVerifier.h
