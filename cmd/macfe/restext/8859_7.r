 /*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "csid.h"
#include "resgui.h"
data 'xlat' ( xlat_MAC_GREEK_TO_8859_7,  "MAC_GREEK -> 8859_7",purgeable){
/*     Translation MacOS_Greek.txt -> 8859-7.txt   */
/* 80 is unmap !!! */
/* 81 is unmap !!! */
/* 83 is unmap !!! */
/* 85 is unmap !!! */
/* 86 is unmap !!! */
/* 88 is unmap !!! */
/* 89 is unmap !!! */
/* 8A is unmap !!! */
/* 8D is unmap !!! */
/* 8E is unmap !!! */
/* 8F is unmap !!! */
/* 90 is unmap !!! */
/* 91 is unmap !!! */
/* 93 is unmap !!! */
/* 94 is unmap !!! */
/* 95 is unmap !!! */
/* 96 is unmap !!! */
/* 98 is unmap !!! */
/* 99 is unmap !!! */
/* 9A is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* 9F is unmap !!! */
/* A0 is unmap !!! */
/* A7 is unmap !!! */
/* A8 is unmap !!! */
/* AD is unmap !!! */
/* AF is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B4 is unmap !!! */
/* C5 is unmap !!! */
/* C9 is unmap !!! */
/* CF is unmap !!! */
/* D0 is unmap !!! */
/* D2 is unmap !!! */
/* D3 is unmap !!! */
/* D4 is unmap !!! */
/* D5 is unmap !!! */
/* D6 is unmap !!! */
/* FF is unmap !!! */
/* There are total 41 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 B283 B385 86B5 8889 8AB4 A88D 8E8F"
/*9x*/  $"9091 A393 9495 B7BD 9899 9AA6 AD9D 9E9F"
/*Ax*/  $"A0C3 C4C8 CBCE D0A7 A8A9 D3DA A7AD B0AF"
/*Bx*/  $"C1B1 B2B3 B4C2 C5C6 C7C9 CACC D6DB D8D9"
/*Cx*/  $"DCCD ACCF D1C5 D4AB BBC9 A0D5 D7B6 B8CF"
/*Dx*/  $"D0AF D2D3 A1A2 D6B9 BABC BEDD DEDF FCBF"
/*Ex*/  $"FDE1 E2F8 E4E5 F6E3 E7E9 EEEA EBEC EDEF"
/*Fx*/  $"F0FE F1F3 F4E8 F9F2 F7F5 E6FA FBC0 E0FF"
};

data 'xlat' ( xlat_8859_7_TO_MAC_GREEK,  "8859_7 -> MAC_GREEK",purgeable){
/*     Translation 8859-7.txt -> MacOS_Greek.txt   */
/* 80 is unmap !!! */
/* 81 is unmap !!! */
/* 82 is unmap !!! */
/* 83 is unmap !!! */
/* 84 is unmap !!! */
/* 85 is unmap !!! */
/* 86 is unmap !!! */
/* 87 is unmap !!! */
/* 88 is unmap !!! */
/* 89 is unmap !!! */
/* 8A is unmap !!! */
/* 8B is unmap !!! */
/* 8C is unmap !!! */
/* 8D is unmap !!! */
/* 8E is unmap !!! */
/* 8F is unmap !!! */
/* 90 is unmap !!! */
/* 91 is unmap !!! */
/* 92 is unmap !!! */
/* 93 is unmap !!! */
/* 94 is unmap !!! */
/* 95 is unmap !!! */
/* 96 is unmap !!! */
/* 97 is unmap !!! */
/* 98 is unmap !!! */
/* 99 is unmap !!! */
/* 9A is unmap !!! */
/* 9B is unmap !!! */
/* 9C is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* 9F is unmap !!! */
/* A1 is unmap !!! */
/* A2 is unmap !!! */
/* A4 is unmap !!! */
/* A5 is unmap !!! */
/* AA is unmap !!! */
/* AE is unmap !!! */
/* B7 is unmap !!! */
/* D2 is unmap !!! */
/* FF is unmap !!! */
/* There are total 41 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*9x*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"
/*Ax*/  $"CAD4 D592 A4A5 9BAC 8CA9 AAC7 C29C AED1"
/*Bx*/  $"AEB1 8284 8B87 CD96 CED7 D8C8 D997 DADF"
/*Cx*/  $"FDB0 B5A1 A2B6 B7B8 A3B9 BAA4 BBC1 A5C3"
/*Dx*/  $"A6C4 D2AA C6CB BCCC BEBF ABBD C0DB DCDD"
/*Ex*/  $"FEE1 E2E7 E4E5 FAE8 F5E9 EBEC EDEE EAEF"
/*Fx*/  $"F0F2 F7F3 F4F9 E6F8 E3F6 FBFC DEE0 F1FF"
};
data 'xlat' ( xlat_MAC_GREEK_TO_CP_1253,  "MAC_GREEK -> CP_1253",purgeable){
/*     Translation ../fromftp/MacOS_Greek.txt -> ../fromftp/cp1253.x   */
/* 80 is unmap !!! */
/* 81 is unmap !!! */
/* 83 is unmap !!! */
/* 85 is unmap !!! */
/* 86 is unmap !!! */
/* 88 is unmap !!! */
/* 89 is unmap !!! */
/* 8A is unmap !!! */
/* 8D is unmap !!! */
/* 8E is unmap !!! */
/* 8F is unmap !!! */
/* 90 is unmap !!! */
/* 91 is unmap !!! */
/* 94 is unmap !!! */
/* 95 is unmap !!! */
/* 99 is unmap !!! */
/* 9A is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* 9F is unmap !!! */
/* A7 is unmap !!! */
/* AD is unmap !!! */
/* AF is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* C5 is unmap !!! */
/* CF is unmap !!! */
/* D6 is unmap !!! */
/* FF is unmap !!! */
/* There are total 29 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 B283 B385 86A1 8889 8AB4 A88D 8E8F"
/*9x*/  $"9091 A399 9495 95BD 8999 9AA6 AD9D 9E9F"
/*Ax*/  $"86C3 C4C8 CBCE D0A7 AEA9 D3DA A7AD B0AF"
/*Bx*/  $"C1B1 B2B3 A5C2 C5C6 C7C9 CACC D6DB D8D9"
/*Cx*/  $"DCCD ACCF D1C5 D4AB BB85 A0D5 D7A2 B8CF"
/*Dx*/  $"96AF 9394 9192 D6B9 BABC BEDD DEDF FCBF"
/*Ex*/  $"FDE1 E2F8 E4E5 F6E3 E7E9 EEEA EBEC EDEF"
/*Fx*/  $"F0FE F1F3 F4E8 F9F2 F7F5 E6FA FBC0 E0FF"
};

data 'xlat' ( xlat_CP_1253_TO_MAC_GREEK,  "CP_1253 -> MAC_GREEK",purgeable){
/*     Translation ../fromftp/cp1253.x -> ../fromftp/MacOS_Greek.txt   */
/* 80 is unmap !!! */
/* 81 is unmap !!! */
/* 82 is unmap !!! */
/* 83 is unmap !!! */
/* 84 is unmap !!! */
/* 87 is unmap !!! */
/* 88 is unmap !!! */
/* 8A is unmap !!! */
/* 8B is unmap !!! */
/* 8C is unmap !!! */
/* 8D is unmap !!! */
/* 8E is unmap !!! */
/* 8F is unmap !!! */
/* 90 is unmap !!! */
/* 97 is unmap !!! */
/* 98 is unmap !!! */
/* 9A is unmap !!! */
/* 9B is unmap !!! */
/* 9C is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* 9F is unmap !!! */
/* A4 is unmap !!! */
/* AA is unmap !!! */
/* B5 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* D2 is unmap !!! */
/* FF is unmap !!! */
/* There are total 29 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 8283 84C9 A087 8898 8A8B 8C8D 8E8F"
/*9x*/  $"90D4 D5D2 D396 D0D1 9893 9A9B 9C9D 9E9F"
/*Ax*/  $"CA87 CD92 A4B4 9BAC 8CA9 AAC7 C29C A8D1"
/*Bx*/  $"AEB1 8284 8BB5 B696 CED7 D8C8 D997 DADF"
/*Cx*/  $"FDB0 B5A1 A2B6 B7B8 A3B9 BAA4 BBC1 A5C3"
/*Dx*/  $"A6C4 D2AA C6CB BCCC BEBF ABBD C0DB DCDD"
/*Ex*/  $"FEE1 E2E7 E4E5 FAE8 F5E9 EBEC EDEE EAEF"
/*Fx*/  $"F0F2 F7F3 F4F9 E6F8 E3F6 FBFC DEE0 F1FF"
};
