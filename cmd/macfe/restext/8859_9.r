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
data 'xlat' ( xlat_MAC_TURKISH_TO_8859_9,  "MAC_TURKISH -> 8859_9",purgeable){
/*     Translation MacOS_Turkish.txt -> 8859-9.txt   */
/* A0 is unmap !!! */
/* A5 is unmap !!! */
/* AA is unmap !!! */
/* AD is unmap !!! */
/* B0 is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* B8 is unmap !!! */
/* B9 is unmap !!! */
/* BA is unmap !!! */
/* BD is unmap !!! */
/* C3 is unmap !!! */
/* C4 is unmap !!! */
/* C5 is unmap !!! */
/* C6 is unmap !!! */
/* C9 is unmap !!! */
/* CE is unmap !!! */
/* CF is unmap !!! */
/* D0 is unmap !!! */
/* D1 is unmap !!! */
/* D2 is unmap !!! */
/* D3 is unmap !!! */
/* D4 is unmap !!! */
/* D5 is unmap !!! */
/* D7 is unmap !!! */
/* D9 is unmap !!! */
/* E0 is unmap !!! */
/* E2 is unmap !!! */
/* E3 is unmap !!! */
/* E4 is unmap !!! */
/* F0 is unmap !!! */
/* F5 is unmap !!! */
/* F6 is unmap !!! */
/* F7 is unmap !!! */
/* F9 is unmap !!! */
/* FA is unmap !!! */
/* FB is unmap !!! */
/* FD is unmap !!! */
/* FE is unmap !!! */
/* FF is unmap !!! */
/* There are total 42 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"C4C5 C7C9 D1D6 DCE1 E0E2 E4E3 E5E7 E9E8"
/*9x*/  $"EAEB EDEC EEEF F1F3 F2F4 F6F5 FAF9 FBFC"
/*Ax*/  $"A0B0 A2A3 A7A5 B6DF AEA9 AAB4 A8AD C6D8"
/*Bx*/  $"B0B1 B2B3 A5B5 B6B7 B8B9 BAAA BABD E6F8"
/*Cx*/  $"BFA1 ACC3 C4C5 C6AB BBC9 A0C0 C3D5 CECF"
/*Dx*/  $"D0D1 D2D3 D4D5 F7D7 FFD9 D0F0 DDFD DEFE"
/*Ex*/  $"E0B7 E2E3 E4C2 CAC1 CBC8 CDCE CFCC D3D4"
/*Fx*/  $"F0D2 DADB D9F5 F6F7 AFF9 FAFB B8FD FEFF"
};

data 'xlat' ( xlat_8859_9_TO_MAC_TURKISH,  "8859_9 -> MAC_TURKISH",purgeable){
/*     Translation 8859-9.txt -> MacOS_Turkish.txt   */
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
/* A4 is unmap !!! */
/* A6 is unmap !!! */
/* AD is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B9 is unmap !!! */
/* BC is unmap !!! */
/* BD is unmap !!! */
/* BE is unmap !!! */
/* D7 is unmap !!! */
/* There are total 42 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*9x*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"
/*Ax*/  $"CAC1 A2A3 A4B4 A6A4 ACA9 BBC7 C2AD A8F8"
/*Bx*/  $"A1B1 B2B3 ABB5 A6E1 FCB9 BCC8 BCBD BEC0"
/*Cx*/  $"CBE7 E5CC 8081 AE82 E983 E6E8 EDEA EBEC"
/*Dx*/  $"DA84 F1EE EFCD 85D7 AFF4 F2F3 86DC DEA7"
/*Ex*/  $"8887 898B 8A8C BE8D 8F8E 9091 9392 9495"
/*Fx*/  $"DB96 9897 999B 9AD6 BF9D 9C9E 9FDD DFD8"
};

