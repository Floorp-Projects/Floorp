/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsUnicodeMappingUtil.h"
#include "nsDeviceContextMac.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsCRT.h"
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define BAD_FONT_NUM -1 
//--------------------------------------------------------------------------

nsUnicodeMappingUtil *nsUnicodeMappingUtil::gSingleton = nsnull;
//--------------------------------------------------------------------------

#ifdef DEBUG
static int gUnicodeMappingUtilCount = 0;
#endif

int PR_CALLBACK nsUnicodeMappingUtil::PrefChangedCallback( const char* aPrefName, void* instance_data)
{
	//printf("PrefChangeCallback \n");
	nsUnicodeMappingUtil::GetSingleton()->Reset();
	return 0;
}

MOZ_DECL_CTOR_COUNTER(nsUnicodeMappingUtil)

nsUnicodeMappingUtil::nsUnicodeMappingUtil()
{
	Init();
	MOZ_COUNT_CTOR(nsUnicodeMappingUtil);
#ifdef DEBUG
	++gUnicodeMappingUtilCount;
	NS_ASSERTION(gUnicodeMappingUtilCount == 1, "not singleton");
#endif
}
void nsUnicodeMappingUtil::Reset()
{
	CleanUp();
	Init();
}
void nsUnicodeMappingUtil::Init()
{
	InitGenericFontMapping();
	InitFromPref();
	InitScriptFontMapping();
	InitBlockToScriptMapping();
}
void nsUnicodeMappingUtil::CleanUp()
{
	for(int i= 0 ; i < smPseudoTotalScripts; i ++) {
		for(int j=0; j < 5; j++) {
			if(mGenericFontMapping[i][j])
	  		 delete mGenericFontMapping[i][j];
	  	}
	}
}
//--------------------------------------------------------------------------

nsUnicodeMappingUtil::~nsUnicodeMappingUtil()
{
	CleanUp();

#ifdef DEBUG
	--gUnicodeMappingUtilCount;
#endif
	mPref->UnregisterCallback("font.name.", nsUnicodeMappingUtil::PrefChangedCallback, (void*) nsnull);
	MOZ_COUNT_DTOR(nsUnicodeMappingUtil);
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
	mGenericFontMapping[smArabic][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Lucida Grande") );
	mGenericFontMapping[smArabic][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Lucida Grande") );
	mGenericFontMapping[smArabic][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Monaco") );

	// smHebrew
	mGenericFontMapping[smHebrew][kSerif]     = new nsAutoString( NS_LITERAL_STRING("Lucida Grande") );
	mGenericFontMapping[smHebrew][kSansSerif] = new nsAutoString( NS_LITERAL_STRING("Lucida Grande") );
	mGenericFontMapping[smHebrew][kMonospace] = new nsAutoString( NS_LITERAL_STRING("Monaco") );

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
        // No separate script code for zh-HK. Use smTradChinese.
	if(0==nsCRT::strcmp(aLangGroup,  "zh-HK")) {
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
  
  PRInt32 p1 = curPrefName.RFindChar('.');
  if(-1==p1)
  	return;
  PRInt32 p2 = curPrefName.RFindChar('.', p1-1);
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
  
  nsString genNameString;
  genNameString.AssignWithConversion(genName);
  nsGenericFontNameType type = Self->MapGenericFontNameType(genNameString);
  if(type >= kUknownGenericFontName)
  	return;
  	
  char* valueInUTF8 = nsnull;
  Self->mPref->CopyCharPref(aName, &valueInUTF8);
  if(!valueInUTF8)
    return;
  if(!*valueInUTF8)
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
  if( Self->mGenericFontMapping[script][type] )
  	delete Self->mGenericFontMapping[script][type];
  Self->mGenericFontMapping[script][type] = fontname;
#ifdef DEBUG_ftang_font
  char* utf8 = ToNewUTF8String(*fontname);
  printf("font %d %d %s= %s\n",script , type, aName,utf8);
  if (utf8)
    Recycle(utf8); 
#endif
}
void nsUnicodeMappingUtil::InitFromPref()
{
  if (!mPref) {
    mPref = do_GetService(kPrefCID);
    if (!mPref)
      return;
    mPref->RegisterCallback("font.name.", nsUnicodeMappingUtil::PrefChangedCallback, (void*) nsnull);
  }
  mPref->EnumerateChildren("font.name.", nsUnicodeMappingUtil::PrefEnumCallback, this);
  
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

			mPref->CopyCharPref (theNeededPreference,&valueInUTF8);

			if (valueInUTF8)
			{
				if (!*valueInUTF8)
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
		}
		else {
			long fondsize = ::GetScriptVariable(script, smScriptPrefFondSize);
			if (!fondsize)
			  fondsize = ::GetScriptVariable(smUnicodeScript, smScriptPrefFondSize);

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
    mBlockToScriptMapping[i] = prebuildMapping[i];
  }
}

//--------------------------------------------------------------------------
nsGenericFontNameType nsUnicodeMappingUtil::MapGenericFontNameType(const nsString& aGenericName)
{
	if (aGenericName.LowerCaseEqualsLiteral("serif"))
	  return kSerif;
	if (aGenericName.LowerCaseEqualsLiteral("sans-serif"))
	  return kSansSerif;
	if (aGenericName.LowerCaseEqualsLiteral("monospace"))
	  return kMonospace;
	if (aGenericName.LowerCaseEqualsLiteral("-moz-fixed"))
	  return kMonospace;
	if (aGenericName.LowerCaseEqualsLiteral("cursive"))
	  return kCursive;
	if (aGenericName.LowerCaseEqualsLiteral("fantasy"))
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

/* static */ void
nsUnicodeMappingUtil::FreeSingleton()
{
    delete gSingleton;
    gSingleton = nsnull;
}

//--------------------------------------------------------------------------
