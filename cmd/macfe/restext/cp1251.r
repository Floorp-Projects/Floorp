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
data 'xlat' ( xlat_MAC_CYRILLIC_TO_CP_1251,  "MAC_CYRILLIC -> CP_1251",purgeable){
/*     Translation MacOS_Cyrillic.txt -> cp1251.x   */
/* A2 is unmap !!! */
/* A3 is unmap !!! */
/* AD is unmap !!! */
/* B0 is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B6 is unmap !!! */
/* C3 is unmap !!! */
/* C4 is unmap !!! */
/* C5 is unmap !!! */
/* C6 is unmap !!! */
/* D6 is unmap !!! */
/* There are total 12 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"C0C1 C2C3 C4C5 C6C7 C8C9 CACB CCCD CECF"
/*9x*/  $"D0D1 D2D3 D4D5 D6D7 D8D9 DADB DCDD DEDF"
/*Ax*/  $"86B0 A2A3 A795 B6B2 AEA9 9980 90AD 8183"
/*Bx*/  $"B0B1 B2B3 B3B5 B6A3 AABA AFBF 8A9A 8C9C"
/*Cx*/  $"BCBD ACC3 C4C5 C6AB BB85 A08E 9E8D 9DBE"
/*Dx*/  $"9697 9394 9192 D684 A1A2 8F9F B9A8 B8FF"
/*Ex*/  $"E0E1 E2E3 E4E5 E6E7 E8E9 EAEB ECED EEEF"
/*Fx*/  $"F0F1 F2F3 F4F5 F6F7 F8F9 FAFB FCFD FEA4"
};

data 'xlat' ( xlat_CP_1251_TO_MAC_CYRILLIC,  "CP_1251 -> MAC_CYRILLIC",purgeable){
/*     Translation cp1251.x -> MacOS_Cyrillic.txt   */
/* 82 is unmap !!! */
/* 87 is unmap !!! */
/* 88 is unmap !!! */
/* 89 is unmap !!! */
/* 8B is unmap !!! */
/* 98 is unmap !!! */
/* 9B is unmap !!! */
/* A5 is unmap !!! */
/* A6 is unmap !!! */
/* AD is unmap !!! */
/* B4 is unmap !!! */
/* B7 is unmap !!! */
/* There are total 12 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"ABAE 82AF D7C9 A087 8889 BC8B BECD CBDA"
/*9x*/  $"ACD4 D5D2 D3A5 D0D1 98AA BD9B BFCE CCDB"
/*Ax*/  $"CAD8 D9B7 FFA5 A6A4 DDA9 B8C7 C2AD A8BA"
/*Bx*/  $"A1B1 A7B4 B4B5 A6B7 DEDC B9C8 C0C1 CFBB"
/*Cx*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*Dx*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"
/*Ex*/  $"E0E1 E2E3 E4E5 E6E7 E8E9 EAEB ECED EEEF"
/*Fx*/  $"F0F1 F2F3 F4F5 F6F7 F8F9 FAFB FCFD FEDF"
};

