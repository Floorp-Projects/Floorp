/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsUnicodeMappingUtil.h"
#include "nsUnicodeFontMappingCache.h"
#define BAD_FONT_NUM -1 
//--------------------------------------------------------------------------

nsUnicodeMappingUtil *nsUnicodeMappingUtil::gSingleton = nsnull;
//--------------------------------------------------------------------------

nsUnicodeMappingUtil::nsUnicodeMappingUtil()
{
	InitScriptEnabled();
	InitGenericFontMapping();
	InitScriptFontMapping();
	InitBlockToScriptMapping(); // this must be called after InitScriptEnabled()
	gCache = new nsUnicodeFontMappingCache();
}
//--------------------------------------------------------------------------

nsUnicodeMappingUtil::~nsUnicodeMappingUtil()
{
	for(int i= 0 ; i < 32; i ++) {
		for(int j=0; j < 5; j++) {
			if(mGenericFontMapping[i][j])
	  		 delete mGenericFontMapping[i][j];
	  	}
	}
	if(gCache)
		delete gCache;
}

//--------------------------------------------------------------------------

void nsUnicodeMappingUtil::InitScriptEnabled()
{
	PRUint8 numOfScripts = ::GetScriptManagerVariable(smEnabled);
	ScriptCode script, gotScripts;
	mScriptEnabled = 0;
	PRUint32 scriptMask = 1;
	for(script = gotScripts = 0; 
		((script < 32) && ( gotScripts < numOfScripts)) ; 
			script++, scriptMask <<= 1)
	{
		if(::GetScriptVariable(script, smScriptEnabled))
			mScriptEnabled |= scriptMask;
	}
}
//--------------------------------------------------------------------------

void nsUnicodeMappingUtil::InitGenericFontMapping()
{
	for(int i= 0 ; i < 32; i ++)
		for(int j=0; j < 5; j++)
	  		 mGenericFontMapping[i][j] = nsnull;

	// We probabaly should put the following info into resource ....
	// smRoman
	static nsAutoString times_str("Times");
	static nsAutoString helvetica_str("Helvetica");
	static nsAutoString courier_str("Courier");
	static nsAutoString zapfChancery_str("Zapf Chancery");
	static nsAutoString newCenturySchlbk_str("New Century Schlbk");
	mGenericFontMapping[smRoman][kSerif] = & times_str;
	mGenericFontMapping[smRoman][kSansSerif] = & helvetica_str; // note: MRJ use Geneva for Sans-Serif
	mGenericFontMapping[smRoman][kMonospace] = & courier_str;
	mGenericFontMapping[smRoman][kCursive] = & zapfChancery_str;
	mGenericFontMapping[smRoman][kFantasy] = & newCenturySchlbk_str;

	// smJapanese
	static PRUnichar jfontname1[] = {
	0x672C, 0x660E, 0x671D, 0x2212, 0xFF2D, 0x0000 //    –{–¾’©|‚l
	};
	static PRUnichar jfontname2[] = {
	0x4E38, 0x30B4, 0x30B7, 0x30C3, 0x30AF, 0x2212, 0xFF2D, 0x0000 //    ŠÛƒSƒVƒbƒN|‚l
	};
	static PRUnichar jfontname3[] = {
	0x004F, 0x0073, 0x0061, 0x006B, 0x0061, 0x2212, 0x7B49, 0x5E45, 0x0000 //    Osaka|“™•
	};   
	static nsAutoString nsJFont1(jfontname1);
	static nsAutoString nsJFont2(jfontname2);
	static nsAutoString nsJFont3(jfontname3);
	mGenericFontMapping[smJapanese][kSerif] = &nsJFont1;
	mGenericFontMapping[smJapanese][kSansSerif] = &nsJFont2; 
	mGenericFontMapping[smJapanese][kMonospace] = &nsJFont3;

	// smTradChinese
	static nsAutoString appleLiSungLight_str("Apple LiSung Light");
	static nsAutoString appleLiGothicMedium_str("Apple LiGothic Medium");
	mGenericFontMapping[smTradChinese][kSerif] = & appleLiSungLight_str;
	mGenericFontMapping[smTradChinese][kSansSerif] 
	= mGenericFontMapping[smTradChinese][kMonospace] = & appleLiGothicMedium_str;

	// smKorean
	static nsAutoString appleMyungjo_str("AppleMyungjo");
	static nsAutoString appleGothic_str("AppleGothic");
	mGenericFontMapping[smKorean][kSerif] = & appleMyungjo_str;
	mGenericFontMapping[smKorean][kSansSerif]
	= mGenericFontMapping[smKorean][kMonospace] = &appleGothic_str;

	// smArabic
	static PRUnichar afontname1[] = {
	0x062B, 0x0644, 0x062B, 0x202E, 0x0020, 0x202C, 0x0623, 0x0628, 0x064A, 0x0636, 0x0000 //    ËäË ÃÈêÖ
	};
	static PRUnichar afontname2[] = {
	0x0627, 0x0644, 0x0628, 0x064A, 0x0627, 0x0646, 0x0000 //    ÇäÈêÇæ
	};
	static PRUnichar afontname3[] = {
	0x062C, 0x064A, 0x0632, 0x0629, 0x0000 //    ÌêÒÉ
	};   
	static nsAutoString nsAFont1(afontname1);
	static nsAutoString nsAFont2(afontname2);
	static nsAutoString nsAFont3(afontname3);
	mGenericFontMapping[smArabic][kSerif] = &nsAFont1;
	mGenericFontMapping[smArabic][kSansSerif] = &nsAFont2; 
	mGenericFontMapping[smArabic][kMonospace] = &nsAFont3;

	// smHebrew
	static PRUnichar hfontname1[] = {
	0x05E4, 0x05E0, 0x05D9, 0x05E0, 0x05D9, 0x05DD, 0x202E, 0x0020, 0x202C, 0x05D7, 0x05D3, 0x05E9, 0x0000 //    ôðéðéí çãù
	};
	static PRUnichar hfontname2[] = {
	0x05D0, 0x05E8, 0x05D9, 0x05D0, 0x05DC, 0x0000 //    àøéàì
	};
	static nsAutoString nsHFont1(hfontname1);
	static nsAutoString nsHFont2(hfontname2);
	mGenericFontMapping[smHebrew][kSerif] = & nsHFont1;
	mGenericFontMapping[smHebrew][kSansSerif]
	= mGenericFontMapping[smHebrew][kMonospace] = & nsHFont2;

	// smCyrillic
	static nsAutoString latinski_str("Latinski");
	static nsAutoString pryamoyProp_str("Pryamoy Prop");
	static nsAutoString apcCourier_str("APC Courier");
	mGenericFontMapping[smCyrillic][kSerif] = &latinski_str;
	mGenericFontMapping[smCyrillic][kSansSerif] = &pryamoyProp_str;
	mGenericFontMapping[smCyrillic][kMonospace] = &apcCourier_str;

	// smDevanagari
	static nsAutoString devanagariMT_str("Devanagari MT");
	mGenericFontMapping[smDevanagari][kSerif]
	= mGenericFontMapping[smDevanagari][kSansSerif]
	= mGenericFontMapping[smDevanagari][kMonospace] = & devanagariMT_str;

	// smGurmukhi
	static nsAutoString gurukhiMT_str("Gurmukhi MT");
	mGenericFontMapping[smGurmukhi][kSerif]
	= mGenericFontMapping[smGurmukhi][kSansSerif]
	= mGenericFontMapping[smGurmukhi][kMonospace] = & gurukhiMT_str;

	// smGujarati
	static nsAutoString gujaratiMT_str("Gujarati MT");
	mGenericFontMapping[smGujarati][kSerif]
	= mGenericFontMapping[smGujarati][kSansSerif]
	= mGenericFontMapping[smGujarati][kMonospace] = & gujaratiMT_str;

	// smThai
	static nsAutoString thonburi_str("Thonburi");
	static nsAutoString krungthep_str("Krungthep");
	static nsAutoString ayuthaya_str("Ayuthaya");
	mGenericFontMapping[smThai][kSerif] = &thonburi_str;
	mGenericFontMapping[smThai][kSansSerif] = &krungthep_str;
	mGenericFontMapping[smThai][kMonospace] = &ayuthaya_str;

	// smSimpChinese
	static nsAutoString song_str("Song");
	static nsAutoString hei_str("Hei");
	mGenericFontMapping[smSimpChinese][kSerif] = &song_str;
	mGenericFontMapping[smSimpChinese][kSansSerif]
	= mGenericFontMapping[smSimpChinese][kMonospace] = & hei_str;

	// smCentralEuroRoman
	static nsAutoString timesCE_str("Times CE");
	static nsAutoString helveticaCE_str("Helvetica CE");
	static nsAutoString courierCE_str("Courier CE");
	mGenericFontMapping[smCentralEuroRoman][kSerif] = &timesCE_str;
	mGenericFontMapping[smCentralEuroRoman][kSansSerif] = &helveticaCE_str;
	mGenericFontMapping[smCentralEuroRoman][kMonospace] = &courierCE_str;
}
//--------------------------------------------------------------------------

void nsUnicodeMappingUtil::InitScriptFontMapping()
{
	// Get font from Script manager
	for(ScriptCode script = 0; script< 32 ; script++)
	{
		mScriptFontMapping[script] = BAD_FONT_NUM;			
		short fontNum;
		if(ScriptEnabled(script)) {
			long fondsize = ::GetScriptVariable(script, smScriptPrefFondSize);
			if((fondsize) && ((fondsize >> 16))) {
				fontNum = (fondsize >> 16);
				mScriptFontMapping[script] = fontNum;
			}
		}
	}
}
//--------------------------------------------------------------------------
void nsUnicodeMappingUtil::InitBlockToScriptMapping()
{
	static ScriptCode prebuildMapping[kUnicodeBlockSize] = 
	{
		// start the fixed section
		smGreek, 	smCyrillic,		smArmenian, 
		smHebrew, 	smArabic, 	smDevanagari,	smBengali, 	
		smGurmukhi, smGujarati, smOriya, 		smTamil, 	
		smTelugu, 	smKannada,	smMalayalam,	smThai,
		smLao,		smTibetan,	smGeorgian,		smKorean,
		smTradChinese,
		
		// start the variable section
		smRoman, 	smRoman,	smJapanese,	smJapanese,		smJapanese,
		smRoman
	};
	for(PRUint32 i= 0; i < kUnicodeBlockSize; i++) 
	{
		if( ScriptEnabled(prebuildMapping[i]) ) {
			mBlockToScriptMapping[i] = prebuildMapping[i];
		} else {
			if(i < kUnicodeBlockFixedScriptMax) {
				mBlockToScriptMapping[i] = (PRInt8) smRoman;
			} else {
				if( (kCJKMisc == i) || (kHiraganaKatakana == i) || (kCJKIdeographs == i) ) {
					// fallback in the following sequence
					// smTradChinese  smKorean smSimpChinese
					mBlockToScriptMapping[i] = (PRInt8)
						( ScriptEnabled(smTradChinese) ? smTradChinese :
							( ScriptEnabled(smKorean) ? smKorean :
								( ScriptEnabled(smSimpChinese) ? smSimpChinese : smRoman )												
							)					
						);
				}				
			}
		}
	}
}

//--------------------------------------------------------------------------
nsGenericFontNameType nsUnicodeMappingUtil::MapGenericFontNameType(const nsString& aGenericName)
{
	if (aGenericName.EqualsIgnoreCase("serif"))
	  return kSerif;
	if (aGenericName.EqualsIgnoreCase("sans-serif"))
	  return kSansSerif;
	if (aGenericName.EqualsIgnoreCase("monospace"))
	  return kMonospace;
	if (aGenericName.EqualsIgnoreCase("cursive"))
	  return kCursive;
	if (aGenericName.EqualsIgnoreCase("fantasy"))
	  return kFantasy;
	  
	return kUknownGenericFontName;			
}

//--------------------------------------------------------------------------


//--------------------------------------------------------------------------

nsUnicodeMappingUtil* nsUnicodeMappingUtil::GetSingleton()
{
	if( ! gSingleton)
		gSingleton = new nsUnicodeMappingUtil();
	return gSingleton;
}
//--------------------------------------------------------------------------
