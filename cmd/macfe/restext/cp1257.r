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
data 'xlat' ( xlat_MAC_CE_TO_CP_1257,  "MAC_CE -> CP_1257",purgeable){
/*     Translation CENTEURO.TXT -> CP1257.TXT   */
/* 10 is unmap !!! */
/* 87 is unmap !!! */
/* 91 is unmap !!! */
/* 92 is unmap !!! */
/* 93 is unmap !!! */
/* 99 is unmap !!! */
/* 9C is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* AD is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* BB is unmap !!! */
/* BC is unmap !!! */
/* BD is unmap !!! */
/* BE is unmap !!! */
/* C3 is unmap !!! */
/* C5 is unmap !!! */
/* C6 is unmap !!! */
/* CB is unmap !!! */
/* CC is unmap !!! */
/* CE is unmap !!! */
/* D7 is unmap !!! */
/* D9 is unmap !!! */
/* DA is unmap !!! */
/* DB is unmap !!! */
/* DE is unmap !!! */
/* E7 is unmap !!! */
/* E8 is unmap !!! */
/* E9 is unmap !!! */
/* EA is unmap !!! */
/* EF is unmap !!! */
/* F1 is unmap !!! */
/* F2 is unmap !!! */
/* F3 is unmap !!! */
/* F4 is unmap !!! */
/* F5 is unmap !!! */
/* F8 is unmap !!! */
/* F9 is unmap !!! */
/* There are total 41 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"C4C2 E2C9 C0D6 DC87 E0C8 E4E8 C3E3 E9CA"
/*9x*/  $"EA91 9293 C7E7 CBF3 EB99 F6F5 9C9D 9EFC"
/*Ax*/  $"86B0 C6A3 A795 B6DF AEA9 99E6 8DAD ECC1"
/*Bx*/  $"E1CE B2B3 EECD B6B7 F9CF EFBB BCBD BED2"
/*Cx*/  $"F2D1 ACC3 F1C5 C6AB BB85 A0CB CCD5 CED4"
/*Dx*/  $"9697 9394 9192 F7D7 F4D9 DADB 8B9B DEAA"
/*Ex*/  $"BAD0 8284 F0DA FAE7 E8E9 EADE FEDB D3EF"
/*Fx*/  $"FBF1 F2F3 F4F5 D8F8 F8F9 EDDD D9FD CC8E"
};

data 'xlat' ( xlat_CP_1257_TO_MAC_CE,  "CP_1257 -> MAC_CE",purgeable){
/*     Translation CP1257.TXT -> CENTEURO.TXT   */
/* 10 is unmap !!! */
/* 80 is unmap !!! */
/* 81 is unmap !!! */
/* 87 is unmap !!! */
/* 88 is unmap !!! */
/* 89 is unmap !!! */
/* 8A is unmap !!! */
/* 8F is unmap !!! */
/* 90 is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* 9F is unmap !!! */
/* A1 is actually unmap " */
/* A2 is unmap !!! */
/* A4 is unmap !!! */
/* A5 is unmap !!! " */
/* A6 is unmap !!! */
/* A8 is unmap !!! */
/* AD is unmap !!! */
/* AF is unmap !!! */
/* B1 is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B4 is unmap !!! */
/* B5 is unmap !!! */
/* B7 is unmap !!! */
/* B8 is unmap !!! */
/* B9 is unmap !!! */
/* BC is unmap !!! */
/* BD is unmap !!! */
/* BE is unmap !!! */
/* BF is unmap !!! */
/* C5 is unmap !!! */
/* D7 is unmap !!! */
/* E5 is unmap !!! */
/* FF is unmap !!! */
/* There are total 35 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 E2E2 E3C9 A087 8889 8ADC DCAC FF8F"
/*9x*/  $"90D4 D5D2 D3A5 D0D1 D1AA AADD DD9D 9E9F"
/*Ax*/  $"CA22 A2A3 A422 A6A4 A8A9 DFC7 C2AD A8AF"
/*Bx*/  $"A1B1 B2B3 B4B5 A6B7 B8B9 E0C8 BCBD BEBF"
/*Cx*/  $"84AF 818C 80C5 A294 8983 8F96 FEB5 B1B9"
/*Dx*/  $"E1C1 BFEE CFCD 85D7 F6FC E5ED 86FB EBA7"
/*Ex*/  $"88B0 828D 8AE5 AB95 8B8E 9098 AEFA B4BA"
/*Fx*/  $"E4C4 C097 D89B 9AD6 F7B8 E6F0 9FFD ECFF"
};

