/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "uintl.h"
#include "resgui.h"
#include "uprefd.h"
#include "intl_csi.h"

INTL_Encoding_ID ScriptToEncoding(ScriptCode script)
{
	switch(script)
	{
		case smRoman:			return CS_MAC_ROMAN;		//	= 0,							/*Roman*/
		case smJapanese:		return CS_SJIS;				//	= 1,							/*Japanese*/
		case smTradChinese:		return CS_BIG5;				//	= 2,							/*Traditional Chinese*/
		case smKorean:			return CS_KSC_8BIT;			//	= 3,							/*Korean*/
//		case smArabic:					//	= 4,							/*Arabic*/
//		case smHebrew:					//	= 5,							/*Hebrew*/
		case smGreek:			return CS_MAC_GREEK;		//	= 6,							/*Greek*/
		case smCyrillic:		return CS_MAC_CYRILLIC;		//	= 7,							/*Cyrillic*/
//		case smRSymbol:					//	= 8,							/*Right-left symbol*/
//		case smDevanagari:				//	= 9,							/*Devanagari*/
//		case smGurmukhi:				//	= 10,							/*Gurmukhi*/
//		case smGujarati:				//	= 11,							/*Gujarati*/
//		case smOriya:					//	= 12,							/*Oriya*/
//		case smBengali:					//	= 13,							/*Bengali*/
//		case smTamil:					//	= 14,							/*Tamil*/
//		case smTelugu:					//	= 15,							/*Telugu*/
//		case smKannada:					//	= 16,							/*Kannada/Kanarese*/
//		case smMalayalam:				//	= 17							/*Malayalam*/
//		case smSinhalese:				//	= 18,							/*Sinhalese*/
//		case smBurmese:					//	= 19,							/*Burmese*/
//		case smKhmer:					//	= 20,							/*Khmer/Cambodian*/
//		case smThai:					//	= 21,							/*Thai*/
//		case smLaotian:					//	= 22,							/*Laotian*/
//		case smGeorgian:				//	= 23,							/*Georgian*/
//		case smArmenian:				//	= 24,							/*Armenian*/
		case smSimpChinese:		return CS_GB_8BIT;			//	= 25,							/*Simplified Chinese*/
//		case smTibetan:					//	= 26,							/*Tibetan*/
//		case smMongolian:				//	= 27,							/*Mongolian*/
//		case smGeez:					//	= 28,							/*Geez/Ethiopic*/
//		case smEthiopic:				//	= 28,							/*Synonym for smGeez*/
		case smEastEurRoman:	return CS_MAC_CE;			//	= 29,							/*Synonym for smSlavic*/
//		case smVietnamese:				//	= 30,							/*Vietnamese*/
//		case smExtArabic:				//	= 31,							/*extended Arabic*/
//		case smUninterp:				//	= 32,							/*uninterpreted symbols, e.g. palette symbols*/
		default:	return CS_MAC_ROMAN;
	}
}

// Returns default document csid which is the current
// selection of the encoding menu.
//
uint16 FE_DefaultDocCharSetID(iDocumentContext context)
{
#pragma unused(context) 

	uint16		csid;
	CommandT	iCommand;
	ResIDT		outMENUid;
	MenuHandle	outMenuHandle;
	Int16		outItem;
	CharParameter	markChar;

	for (csid = CS_DEFAULT, iCommand = ENCODING_BASE; iCommand <= ENCODING_CEILING; iCommand++)
	{
		LMenuBar::GetCurrentMenuBar()->FindMenuItem(iCommand, outMENUid, outMenuHandle, outItem);
		::GetItemMark	(outMenuHandle, outItem, &markChar);
		if (checkMark == markChar)
		{
			csid = (uint16) CPrefs::CmdNumToDocCsid(iCommand);
			break;
		}
	}
	XP_ASSERT(csid != CS_DEFAULT);	// no check mark in the encoding menu
	
	return csid;
}
