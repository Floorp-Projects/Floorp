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
data 'xlat' ( xlat_KOI8_R_TO_MAC_CYRILLIC,  "KOI8_R -> MAC_CYRILLIC",purgeable){
/*     Translation koi8r.txt -> MacOS_Cyrillic.txt   */
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
/* 9B is unmap !!! */
/* 9D is unmap !!! */
/* 9E is unmap !!! */
/* A0 is unmap !!! */
/* A1 is unmap !!! */
/* A2 is unmap !!! */
/* A4 is unmap !!! */
/* A5 is unmap !!! */
/* A6 is unmap !!! */
/* A7 is unmap !!! */
/* A8 is unmap !!! */
/* A9 is unmap !!! */
/* AA is unmap !!! */
/* AB is unmap !!! */
/* AC is unmap !!! */
/* AD is unmap !!! */
/* AE is unmap !!! */
/* AF is unmap !!! */
/* B0 is unmap !!! */
/* B1 is unmap !!! */
/* B2 is unmap !!! */
/* B4 is unmap !!! */
/* B5 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* B8 is unmap !!! */
/* B9 is unmap !!! */
/* BA is unmap !!! */
/* BB is unmap !!! */
/* BC is unmap !!! */
/* BD is unmap !!! */
/* BE is unmap !!! */
/* There are total 54 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"8081 8283 8485 8687 8889 8A8B 8C8D 8E8F"
/*9x*/  $"9091 9293 9495 C3C5 B2B3 CA9B A19D 9ED6"
/*Ax*/  $"A0A1 A2DE A4A5 A6A7 A8A9 AAAB ACAD AEAF"
/*Bx*/  $"B0B1 B2DD B4B5 B6B7 B8B9 BABB BCBD BEA9"
/*Cx*/  $"FEE0 E1F6 E4E5 F4E3 F5E8 E9EA EBEC EDEE"
/*Dx*/  $"EFDF F0F1 F2F3 E6E2 FCFB E7F8 FDF9 F7FA"
/*Ex*/  $"9E80 8196 8485 9483 9588 898A 8B8C 8D8E"
/*Fx*/  $"8F9F 9091 9293 8682 9C9B 8798 9D99 979A"
};

data 'xlat' ( xlat_MAC_CYRILLIC_TO_KOI8_R,  "MAC_CYRILLIC -> KOI8_R",purgeable){
/*     Translation MacOS_Cyrillic.txt -> koi8r.txt   */
/* A0 is unmap !!! */
/* A2 is unmap !!! */
/* A3 is unmap !!! */
/* A4 is unmap !!! */
/* A5 is unmap !!! */
/* A6 is unmap !!! */
/* A7 is unmap !!! */
/* A8 is unmap !!! */
/* AA is unmap !!! */
/* AB is unmap !!! */
/* AC is unmap !!! */
/* AD is unmap !!! */
/* AE is unmap !!! */
/* AF is unmap !!! */
/* B0 is unmap !!! */
/* B1 is unmap !!! */
/* B4 is unmap !!! */
/* B5 is unmap !!! */
/* B6 is unmap !!! */
/* B7 is unmap !!! */
/* B8 is unmap !!! */
/* B9 is unmap !!! */
/* BA is unmap !!! */
/* BB is unmap !!! */
/* BC is unmap !!! */
/* BD is unmap !!! */
/* BE is unmap !!! */
/* BF is unmap !!! */
/* C0 is unmap !!! */
/* C1 is unmap !!! */
/* C2 is unmap !!! */
/* C4 is unmap !!! */
/* C6 is unmap !!! */
/* C7 is unmap !!! */
/* C8 is unmap !!! */
/* C9 is unmap !!! */
/* CB is unmap !!! */
/* CC is unmap !!! */
/* CD is unmap !!! */
/* CE is unmap !!! */
/* CF is unmap !!! */
/* D0 is unmap !!! */
/* D1 is unmap !!! */
/* D2 is unmap !!! */
/* D3 is unmap !!! */
/* D4 is unmap !!! */
/* D5 is unmap !!! */
/* D7 is unmap !!! */
/* D8 is unmap !!! */
/* D9 is unmap !!! */
/* DA is unmap !!! */
/* DB is unmap !!! */
/* DC is unmap !!! */
/* FF is unmap !!! */
/* There are total 54 character unmap !! */
/*        x0x1 x2x3 x4x5 x6x7 x8x9 xAxB xCxD xExF   */
/*8x*/  $"E1E2 F7E7 E4E5 F6FA E9EA EBEC EDEE EFF0"
/*9x*/  $"F2F3 F4F5 E6E8 E3FE FBFD FFF9 F8FC E0F1"
/*Ax*/  $"A09C A2A3 A4A5 A6A7 A8BF AAAB ACAD AEAF"
/*Bx*/  $"B0B1 9899 B4B5 B6B7 B8B9 BABB BCBD BEBF"
/*Cx*/  $"C0C1 C296 C497 C6C7 C8C9 9ACB CCCD CECF"
/*Dx*/  $"D0D1 D2D3 D4D5 9FD7 D8D9 DADB DCB3 A3D1"
/*Ex*/  $"C1C2 D7C7 C4C5 D6DA C9CA CBCC CDCE CFD0"
/*Fx*/  $"D2D3 D4D5 C6C8 C3DE DBDD DFD9 D8DC C0FF"
};

