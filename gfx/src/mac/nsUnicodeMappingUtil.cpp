/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsUnicodeMappingUtil.h"
#include "nsUnicodeFontMappingCache.h"
#include "nsDeviceContextMac.h"
#include "nsReadableUtils.h"
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define BAD_FONT_NUM -1 
//--------------------------------------------------------------------------

nsUnicodeMappingUtil *nsUnicodeMappingUtil::gSingleton = nsnull;
//--------------------------------------------------------------------------

static nsIPref* gPref = nsnull;
static int gUnicodeMappingUtilCount = 0;

int PR_CALLBACK nsUnicodeMappingUtil::PrefChangedCallback( const char* aPrefName, void* instance_data)
{
	//printf("PrefChangeCallback \n");
	nsUnicodeMappingUtil::GetSingleton()->Reset();
	return 0;
}

nsUnicodeMappingUtil::nsUnicodeMappingUtil()
{
	Init();
}
void nsUnicodeMappingUtil::Reset()
{
	CleanUp();
	Init();
}
void nsUnicodeMappingUtil::Init()
{
	InitScriptEnabled();
	InitGenericFontMapping();
	InitFromPref();
	InitScriptFontMapping();
	InitBlockToScriptMapping(); // this must be called after InitScriptEnabled()
	mCache = new nsUnicodeFontMappingCache();
	++gUnicodeMappingUtilCount;
}
void nsUnicodeMappingUtil::CleanUp()
{
	for(int i= 0 ; i < smPseudoTotalScripts; i ++) {
		for(int j=0; j < 5; j++) {
			if(mGenericFontMapping[i][j])
	  		 delete mGenericFontMapping[i][j];
	  	}
	}
	if (mCache)
	{
		delete mCache;
		mCache = nsnull;
	}

}
//--------------------------------------------------------------------------

nsUnicodeMappingUtil::~nsUnicodeMappingUtil()
{
	CleanUp();

	if(0 == --gUnicodeMappingUtilCount) {
		gPref->UnregisterCallback("font.name.", nsUnicodeMappingUtil::PrefChangedCallback, (void*) nsnull);
		NS_IF_RELEASE(gPref);
	}
}

//--------------------------------------------------------------------------

void nsUnicodeMappingUtil::InitScriptEnabled()
{
	PRUint8 numOfScripts = ::GetScriptManagerVariable(smEnabled);
	ScriptCode script, gotScripts;
	mScriptEnabled = 0;
	PRUint32 scriptMask = 1;
	for(script = gotScripts = 0; 
		((script < smUninterp) && ( gotScripts < numOfScripts)) ; 
			script++, scriptMask <<= 1)
	{
		if(::GetScriptVariable(script, smScriptEnabled))
			mScriptEnabled |= scriptMask;
	}
}
//--------------------------------------------------------------------------

void nsUnicodeMappingUtil::InitGenericFontMapping()
{
	for(int i= 0 ; i < smPseudoTotalScripts; i ++)
		for(int j=0; j < 5; j++)
	  		 mGenericFontMapping[i][j] = nsnull;

	// We probabaly should put the following info into resource ....
	// smRoman
	mGenericFontMapping[smRoman][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Times") );
	mGenericFontMapping[smRoman][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Helvetica") ); // note: MRJ use Geneva for Sans-Serif
	mGenericFontMapping[smRoman][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Courier") );
	mGenericFontMapping[smRoman][kCursive]   = new nsAutoString( NS_LITERAL_STRING("Zapf Chancery") );
	mGenericFontMapping[smRoman][kFantasy]   = new nsAutoString( NS_LITERAL_STRING("New Century Schlbk") );

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

	mGenericFontMapping[smJapanese][kSerif]     = new nsAutoString(jfontname1);
	mGenericFontMapping[smJapanese][kSansSerif] = new nsAutoString(jfontname2); 
	mGenericFontMapping[smJapanese][kMonospace] = new nsAutoString(jfontname3);

	// smTradChinese
	mGenericFontMapping[smTradChinese][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Apple LiSung Light") );
	mGenericFontMapping[smTradChinese][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Apple LiGothic Medium") );
	mGenericFontMapping[smTradChinese][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Apple LiGothic Medium") );

	// smKorean
	mGenericFontMapping[smKorean][kSerif]     = new nsAutoString( NS_LITERAL_STRING("AppleMyungjo") );
	mGenericFontMapping[smKorean][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("AppleGothic") );
	mGenericFontMapping[smKorean][kMonospace] = new nsAutoString( NS_LITERAL_STRING("AppleGothic") );

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
	mGenericFontMapping[smArabic][kSerif]     = new nsAutoString(afontname1);
	mGenericFontMapping[smArabic][kSansSerif] = new nsAutoString(afontname2);
	mGenericFontMapping[smArabic][kMonospace] = new nsAutoString(afontname3);

	// smHebrew
	static PRUnichar hfontname1[] = {
	0x05E4, 0x05E0, 0x05D9, 0x05E0, 0x05D9, 0x05DD, 0x202E, 0x0020, 0x202C, 0x05D7, 0x05D3, 0x05E9, 0x0000 //    ôðéðéí çãù
	};
	static PRUnichar hfontname2[] = {
	0x05D0, 0x05E8, 0x05D9, 0x05D0, 0x05DC, 0x0000 //    àøéàì
	};

	mGenericFontMapping[smHebrew][kSerif]     = new nsAutoString(hfontname1);
	mGenericFontMapping[smHebrew][kSansSerif] = new nsAutoString(hfontname2);
	mGenericFontMapping[smHebrew][kMonospace] = new nsAutoString(hfontname2);

	// smCyrillic
	mGenericFontMapping[smCyrillic][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Latinski") );
	mGenericFontMapping[smCyrillic][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Pryamoy Prop") );
	mGenericFontMapping[smCyrillic][kMonospace] = new nsAutoString( NS_LITERAL_STRING("APC Courier") );

	// smDevanagari
	mGenericFontMapping[smDevanagari][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Devanagari MT") );
	mGenericFontMapping[smDevanagari][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Devanagari MT") );
	mGenericFontMapping[smDevanagari][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Devanagari MT") );

	// smGurmukhi
	mGenericFontMapping[smGurmukhi][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Gurmukhi MT") );
	mGenericFontMapping[smGurmukhi][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Gurmukhi MT") );
	mGenericFontMapping[smGurmukhi][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Gurmukhi MT") );

	// smGujarati
	mGenericFontMapping[smGujarati][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Gujarati MT") );
	mGenericFontMapping[smGujarati][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Gujarati MT") );
	mGenericFontMapping[smGujarati][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Gujarati MT") );

	// smThai
	mGenericFontMapping[smThai][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Thonburi") );
	mGenericFontMapping[smThai][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Krungthep") );
	mGenericFontMapping[smThai][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Ayuthaya") );

	// smSimpChinese
	mGenericFontMapping[smSimpChinese][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Song") );
	mGenericFontMapping[smSimpChinese][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Hei") );
	mGenericFontMapping[smSimpChinese][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Hei") );

	// smCentralEuroRoman
	mGenericFontMapping[smCentralEuroRoman][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Times CE") );
	mGenericFontMapping[smCentralEuroRoman][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Helvetica CE") );
	mGenericFontMapping[smCentralEuroRoman][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Courier CE") );
}
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//	nsUnicodeMappingUtil::MapLangGroupToScriptCode
//
//	This method normally returns ScriptCode values, but CAN RETURN pseudo-
//	ScriptCode values for Unicode (32) and User-Defined (33)
//--------------------------------------------------------------------------
ScriptCode nsUnicodeMappingUtil::MapLangGroupToScriptCode(const char* aLangGroup)
{
	if(0==nsCRT::strcmp(aLangGroup,  "x-western")) {
		return smRoman;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "x-central-euro")) {
		return smCentralEuroRoman;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "x-cyrillic")) {
		return smCyrillic;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "el")) {
		return smGreek;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "tr")) {
		return smRoman;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "he")) {
		return smHebrew;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "ar")) {
		return smArabic;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "x-baltic")) {
		return smRoman;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "th")) {
		return smThai;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "ja")) {
		return smJapanese;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "zh-CN")) {
		return smSimpChinese;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "ko")) {
		return smKorean;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "zh-TW")) {
		return smTradChinese;
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "x-unicode")) {
		return (smPseudoUnicode);
	} else 
	if(0==nsCRT::strcmp(aLangGroup,  "x-user-def")) {
		return (smPseudoUserDef);
	} else 
	{
		return smRoman;
	}
}

#define FACESIZE 255 // font name is Str255 in Mac script code
void PR_CALLBACK
nsUnicodeMappingUtil::PrefEnumCallback(const char* aName, void* aClosure)
{
	
	nsUnicodeMappingUtil* Self = (nsUnicodeMappingUtil*)aClosure;
  nsCAutoString curPrefName(aName);
  
  PRInt32 p1 = curPrefName.RFindChar('.', PR_TRUE);
  if(-1==p1)
  	return;
  PRInt32 p2 = curPrefName.RFindChar('.', PR_TRUE, p1-1);
  if(-1==p1)
  	return;
  
  nsCAutoString genName("");
  nsCAutoString langGroup("");
  
  curPrefName.Mid(langGroup,  p1+1, curPrefName.Length()-p1-1);
  curPrefName.Mid(genName, p2+1, p1-p2-1);
  
  ScriptCode script = Self->MapLangGroupToScriptCode(langGroup.get());
  if(script >= (smPseudoTotalScripts))
  {
  	// Because of the pseudo-scripts of Unicode and User-Defined, we have to handle
  	// the expanded ScriptCode value.
  	return;
  }
  if((script == smRoman) && !langGroup.Equals(nsCAutoString("x-western"))) {
  	// need special processing for t,r x-baltic, x-usr-defined
  	return;
  }
  
  nsString genNameString ((const nsString &)genName);
  nsGenericFontNameType type = Self->MapGenericFontNameType(genNameString);
  if(type >= kUknownGenericFontName)
  	return;
  	
  char* valueInUTF8 = nsnull;
  gPref->CopyCharPref(aName, &valueInUTF8);
  if((nsnull == valueInUTF8) || (PL_strlen(valueInUTF8) == 0))
  {
	  Recycle(valueInUTF8);
  	return;
  }
  PRUnichar valueInUCS2[FACESIZE]= { 0 };
  PRUnichar format[] = { '%', 's', 0 };
  PRUint32 n = nsTextFormatter::snprintf(valueInUCS2, FACESIZE, format, valueInUTF8);
  Recycle(valueInUTF8);
  if(n == 0)
  	return;     
  nsString *fontname = new nsAutoString(valueInUCS2);
  if(nsnull == fontname)
  	return;
  short fontID=0;
  if( (! nsDeviceContextMac::GetMacFontNumber(*fontname, fontID)) ||
  	  ((script < smUninterp) && (::FontToScript(fontID) != script)))
  {
  	delete fontname;
  	return;
  }
  if( Self->mGenericFontMapping[script][type] )
  	delete Self->mGenericFontMapping[script][type];
  Self->mGenericFontMapping[script][type] = fontname;
#ifdef DEBUG_ftang_font
  char* utf8 = ToNewUTF8String(*fontname);
  printf("font %d %d %s= %s\n",script , type, aName,utf8);
  Recycle(utf8); 
#endif
}
void nsUnicodeMappingUtil::InitFromPref()
{
  if (!gPref) {
    nsServiceManager::GetService(kPrefCID,
      NS_GET_IID(nsIPref), (nsISupports**) &gPref);
    if (!gPref) {
      return;
    }
    gPref->RegisterCallback("font.name.", nsUnicodeMappingUtil::PrefChangedCallback, (void*) nsnull);
  }	  
  gPref->EnumerateChildren("font.name.", nsUnicodeMappingUtil::PrefEnumCallback, this);
  
}
//--------------------------------------------------------------------------

void nsUnicodeMappingUtil::InitScriptFontMapping()
{
	// Get font from Script manager
	for(ScriptCode script = 0; script< smPseudoTotalScripts ; script++)
	{
		mScriptFontMapping[script] = BAD_FONT_NUM;			
		short fontNum;
		if ((smPseudoUnicode == script) || (smPseudoUserDef == script))
		{
			char	*theNeededPreference;

			if (smPseudoUnicode == script)
				theNeededPreference = "font.name.serif.x-unicode";
			else
				theNeededPreference = "font.name.serif.x-user-def";

			char	*valueInUTF8 = nsnull;

			gPref->CopyCharPref (theNeededPreference,&valueInUTF8);

			if ((nsnull == valueInUTF8) || (PL_strlen (valueInUTF8) == 0))
				Recycle (valueInUTF8);
			else
			{
				PRUnichar	valueInUCS2[FACESIZE]= { 0 };
				PRUnichar	format[] = { '%', 's', 0 };
				PRUint32	n = nsTextFormatter::snprintf(valueInUCS2, FACESIZE, format, valueInUTF8);

				Recycle (valueInUTF8);
				if (n != 0)
				{
					nsString	*fontname = new nsAutoString (valueInUCS2);

					if (nsnull != fontname)
					{
						short	fontID = 0;

						if (nsDeviceContextMac::GetMacFontNumber (*fontname,fontID))
							mScriptFontMapping[script] = fontID;

						delete fontname;
					}
				}
			}
		}
		else if(ScriptEnabled(script)) {
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
		smPseudoUserDef,
		
		// start the variable section
		smRoman, 	smRoman,	smJapanese,	smJapanese,		smJapanese,
		smRoman
	};
	for(PRUint32 i= 0; i < kUnicodeBlockSize; i++) 
	{
		if( ScriptEnabled(prebuildMapping[i]) ) {
			mBlockToScriptMapping[i] = prebuildMapping[i];
		} else {
			if (i == kUserDefinedEncoding)
				mBlockToScriptMapping[i] = prebuildMapping[i];
			else if(i < kUnicodeBlockFixedScriptMax) {
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
	if (aGenericName.EqualsIgnoreCase("-moz-fixed"))
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
