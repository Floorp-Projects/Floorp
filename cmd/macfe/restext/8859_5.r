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
data 'xlat' ( xlat_MAC_CYRILLIC_TO_8859_5,  "MAC_CYRILLIC -> 8859_5",purgeable){
/*     Translation MacOS_Cyrillic.txt -> 8859-5.txt   */
/* A0 is unmap !!! */
/* A1 is unmap !!! */
/* A2 is unmap !!! */
/* A3 is unmap !!! */
/* A5 is unmap !!! */
/* A6 is unmap !!! */
/* A8 is unmap !!! */
/* A9 is unmap !!! */
/* AA is unmap !!! */
/* AD is unmap !!! */
/* B0 is unmap !!! */
/* B1 is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B5 is unmap !!! */
/* B6 is unmap !!! */
/* C2 is unmap !!! */
/* C3 is unmap !!! */
/* C4 is unmap !!! */
/* C5 is unmap !!! */
/* C6 is unmap !!! */
/* C7 is unmap !!! */
/* C8 is unmap !!! */
/* C9 is unmap !!! */
/* D0 is unmap !!! */
/* D1 is unmap !!! */
/* D2 is unmap !!! */
/* D3 is unmap !!! */
/* D4 is unmap !!! */
/* D5 is unmap !!! */
/* D6 is unmap !!! */
/* D7 is unmap !!! */
/* FF is unmap !!! */
/* There are total 33 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"B0B1 B2B3 B4B5 B6B7 B8B9 BABB BCBD BEBF"
/*9x*/  $"C0C1 C2C3 C4C5 C6C7 C8C9 CACB CCCD CECF"
/*Ax*/  $"A0A1 A2A3 FDA5 A6A6 A8A9 AAA2 F2AD A3F3"
/*Bx*/  $"B0B1 B2B3 F6B5 B6A8 A4F4 A7F7 A9F9 AAFA"
/*Cx*/  $"F8A5 C2C3 C4C5 C6C7 C8C9 A0AB FBAC FCF5"
/*Dx*/  $"D0D1 D2D3 D4D5 D6D7 AEFE AFFF F0A1 F1EF"
/*Ex*/  $"D0D1 D2D3 D4D5 D6D7 D8D9 DADB DCDD DEDF"
/*Fx*/  $"E0E1 E2E3 E4E5 E6E7 E8E9 EAEB ECED EEFF"
};

data 'xlat' ( xlat_8859_5_TO_MAC_CYRILLIC,  "8859_5 -> MAC_CYRILLIC",purgeable){
/*     Translation 8859-5.txt -> MacOS_Cyrillic.txt   */
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
/* AD is unmap !!! */
/* There are total 33 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*9x*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"
/*Ax*/  $"CADD ABAE B8C1 A7BA B7BC BECB CDAD D8DA"
/*Bx*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*Cx*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"
/*Dx*/  $"E0E1 E2E3 E4E5 E6E7 E8E9 EAEB ECED EEEF"
/*Ex*/  $"F0F1 F2F3 F4F5 F6F7 F8F9 FAFB FCFD FEDF"
/*Fx*/  $"DCDE ACAF B9CF B4BB C0BD BFCC CEA4 D9DB"
};

