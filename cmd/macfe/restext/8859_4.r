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
data 'xlat' ( xlat_MAC_CE_TO_8859_4,  "MAC_CE -> 8859_4",purgeable){
/*     Translation CENTEURO.TXT -> 8859-4.TXT   */
/* 10 is unmap !!! */
/* 8C is unmap !!! C */
/* 8D is unmap !!! c */
/* 8F is unmap !!! Z */
/* 90 is unmap !!! z */
/* 91 is unmap !!! D */
/* 93 is unmap !!! d */
/* 97 is unmap !!! o */
/* 9D is unmap !!! E */
/* 9E is unmap !!! e */
/* A0 is unmap !!! + */
/* A3 is unmap !!! */
/* A5 is unmap !!! * */
/* A6 is unmap !!! */
/* A8 is unmap !!! (R)*/
/* A9 is unmap !!! (C) */
/* AA is unmap !!! [TM] */
/* AD is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* B8 is unmap !!! l */
/* BB is unmap !!! L */
/* BC is unmap !!! l */
/* BD is unmap !!! L */
/* BE is unmap !!! l */
/* C1 is unmap !!! N */
/* C2 is unmap !!! ! */
/* C3 is unmap !!!  */
/* C4 is unmap !!! n */
/* C5 is unmap !!! N */
/* C6 is unmap !!! ^ */
/* C7 is unmap !!! < */
/* C8 is unmap !!! > */
/* C9 is unmap !!! . */
/* CB is unmap !!! n */
/* CC is unmap !!! O */
/* CE is unmap !!! o */
/* D0 is unmap !!! - */
/* D1 is unmap !!! - */
/* D2 is unmap !!! " */
/* D3 is unmap !!! " */
/* D4 is unmap !!! ` */
/* D5 is unmap !!! ' */
/* D7 is unmap !!! */
/* D9 is unmap !!! R */
/* DA is unmap !!! r */
/* DB is unmap !!! R */
/* DC is unmap !!! < */
/* DD is unmap !!! > */
/* DE is unmap !!! r */
/* E2 is unmap !!! , */
/* E3 is unmap !!! " */
/* E5 is unmap !!! S */
/* E6 is unmap !!! s */
/* E8 is unmap !!! T */
/* E9 is unmap !!! t */
/* EE is unmap !!! O */
/* F1 is unmap !!! U */
/* F3 is unmap !!! u */
/* F4 is unmap !!! U */
/* F5 is unmap !!! u */
/* F8 is unmap !!! Y */
/* F9 is unmap !!! y */
/* FB is unmap !!! Z */
/* FC is unmap !!! L */
/* FD is unmap !!! z */
/* There are total 68 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"C4C0 E0C9 A1D6 DCE1 B1C8 E4E8 4363 E95A"
/*9x*/  $"7A44 ED64 AABA CC6F ECF4 F6F5 FA45 65FC"
/*Ax*/  $"2BB0 CAA3 A72A A6DF 5243 AAEA A8AD BBC7"
/*Bx*/  $"E7CF B2B3 EFD3 B6B7 6CA6 B64C 6C4C 6CD1"
/*Cx*/  $"F14E 21C3 6E4E 5E3C 3E2E A06E 4FD5 6FD2"
/*Dx*/  $"2D2D 2223 6027 F7D7 F252 72DB 3C3E 72A3"
/*Ex*/  $"B3A9 2C22 B953 73C1 5474 CDAE BEDE 4FD4"
/*Fx*/  $"FE55 DA75 5575 D9F9 5979 F35A 4CFD 7AB7"
};

data 'xlat' ( xlat_8859_4_TO_MAC_CE,  "8859_4 -> MAC_CE",purgeable){
/*     Translation 8859-4.TXT -> CENTEURO.TXT   */
/* 10 is unmap !!! */
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
/* A2 is unmap !!! */
/* A4 is unmap !!! */
/* A5 is unmap !!! */
/* AC is unmap !!! */
/* AD is unmap !!! */
/* AF is unmap !!! */
/* B2 is unmap !!! */
/* B4 is unmap !!! */
/* B5 is unmap !!! */
/* B8 is unmap !!! */
/* BC is unmap !!! */
/* BD is unmap !!! */
/* BF is unmap !!! */
/* C2 is unmap !!! */
/* C3 is unmap !!! */
/* C5 is unmap !!! */
/* C6 is unmap !!! */
/* CB is unmap !!! */
/* CE is unmap !!! */
/* D0 is unmap !!! */
/* D7 is unmap !!! */
/* D8 is unmap !!! */
/* DB is unmap !!! */
/* DD is unmap !!! */
/* E2 is unmap !!! */
/* E3 is unmap !!! */
/* E5 is unmap !!! */
/* E6 is unmap !!! */
/* EB is unmap !!! */
/* EE is unmap !!! */
/* F0 is unmap !!! */
/* F8 is unmap !!! */
/* FB is unmap !!! */
/* FD is unmap !!! */
/* FF is unmap !!! */
/* There are total 68 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*9x*/  $"9091 9293 9495 9697 9899 9A9B 9C9D 9E9F"
/*Ax*/  $"CA84 A2DF A4A5 B9A4 ACE1 94FE ACAD EBAF"
/*Bx*/  $"A188 B2E0 B4B5 BAFF B8E4 95AE BCBD ECBF"
/*Cx*/  $"81E7 C2C3 80C5 C6AF 8983 A2CB 96EA CEB1"
/*Dx*/  $"D0BF CFB5 EFCD 85D7 D8F6 F2DB 86DD EDA7"
/*Ex*/  $"8287 E2E3 8AE5 E6B0 8B8E ABEB 9892 EEB4"
/*Fx*/  $"F0C0 D8FA 999B 9AD6 F8F7 9CFB 9FFD F0FF"
};

