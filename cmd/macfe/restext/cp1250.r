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
data 'xlat' ( xlat_MAC_CE_TO_CP_1250,  "MAC_CE -> CP_1250",purgeable){
/*     Translation MacOS_CentralEuro.txt -> cp1250.x   */
/* 81 is unmap !!! */
/* 82 is unmap !!! */
/* 94 is unmap !!! */
/* 95 is unmap !!! */
/* 96 is unmap !!! */
/* 98 is unmap !!! */
/* 9B is unmap !!! */
/* A3 is unmap !!! */
/* AD is unmap !!! */
/* AE is unmap !!! */
/* AF is unmap !!! */
/* B0 is unmap !!! */
/* B1 is unmap !!! */
/* B2 is unmap !!! */
/* B3 is unmap !!! */
/* B4 is unmap !!! */
/* B5 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* B9 is unmap !!! */
/* BA is unmap !!! */
/* BF is unmap !!! */
/* C0 is unmap !!! */
/* C3 is unmap !!! */
/* C6 is unmap !!! */
/* CD is unmap !!! */
/* CF is unmap !!! */
/* D7 is unmap !!! */
/* D8 is unmap !!! */
/* DF is unmap !!! */
/* E0 is unmap !!! */
/* ED is unmap !!! */
/* F0 is unmap !!! */
/* F6 is unmap !!! */
/* F7 is unmap !!! */
/* FA is unmap !!! */
/* FE is unmap !!! */
/* There are total 37 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"C481 82C9 A5D6 DCE1 B9C8 E4E8 C6E6 E98F"
/*9x*/  $"9FCF EDEF 9495 96F3 98F4 F69B FACC ECFC"
/*Ax*/  $"86B0 CAA3 A795 B6DF AEA9 99EA A8AD AEAF"
/*Bx*/  $"B0B1 B2B3 B4B5 B6B7 B3B9 BABC BEC5 E5BF"
/*Cx*/  $"C0D1 ACC3 F1D2 C6AB BB85 A0F2 D5CD F5CF"
/*Dx*/  $"9697 9394 9192 F7D7 D8C0 E0D8 8B9B F8DF"
/*Ex*/  $"E08A 8284 9A8C 9CC1 8D9D CD8E 9EED D3D4"
/*Fx*/  $"F0D9 DAF9 DBFB F6F7 DDFD FAAF A3BF FEA1"
};

data 'xlat' ( xlat_CP_1250_TO_MAC_CE,  "CP_1250 -> MAC_CE",purgeable){
/*     Translation cp1250.x -> MacOS_CentralEuro.txt   */
/* 80 is unmap !!! */
/* 81 is unmap !!! */
/* 83 is unmap !!! */
/* 87 is unmap !!! */
/* 88 is unmap !!! */
/* 89 is unmap !!! */
/* 90 is unmap !!! */
/* 98 is unmap !!! */
/* A2 is unmap !!! */
/* A4 is unmap !!! */
/* A6 is unmap !!! */
/* AA is unmap !!! */
/* AD is unmap !!! */
/* B1 is unmap !!! */
/* B2 is unmap !!! */
/* B4 is unmap !!! */
/* B5 is unmap !!! */
/* B7 is unmap !!! */
/* B8 is unmap !!! */
/* BA is unmap !!! */
/* BD is unmap !!! */
/* C2 is unmap !!! */
/* C3 is unmap !!! */
/* C7 is unmap !!! */
/* CB is unmap !!! */
/* CE is unmap !!! */
/* D0 is unmap !!! */
/* D7 is unmap !!! */
/* DE is unmap !!! */
/* E2 is unmap !!! */
/* E3 is unmap !!! */
/* E7 is unmap !!! */
/* EB is unmap !!! */
/* EE is unmap !!! */
/* F0 is unmap !!! */
/* FE is unmap !!! */
/* FF is unmap !!! */
/* There are total 37 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 E283 E3C9 A087 8889 E1DC E5E8 EB8F"
/*9x*/  $"90D4 D5D2 D3A5 D0D1 98AA E4DD E6E9 EC90"
/*Ax*/  $"CAFF A2FC A484 A6A4 ACA9 AAC7 C2AD A8FB"
/*Bx*/  $"A1B1 B2B8 B4B5 A6B7 B888 BAC8 BBBD BCFD"
/*Cx*/  $"D9E7 C2C3 80BD 8CC7 8983 A2CB 9DEA CE91"
/*Dx*/  $"D0C1 C5EE EFCC 85D7 DBF1 F2F4 86F8 DEA7"
/*Ex*/  $"DA87 E2E3 8ABE 8DE7 8B8E ABEB 9E92 EE93"
/*Fx*/  $"F0C4 CB97 99CE 9AD6 DEF3 9CF5 9FF9 FEFF"
};

