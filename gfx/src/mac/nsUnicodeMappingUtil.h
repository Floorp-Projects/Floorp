/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include <Script.h>

#include "prtypes.h"
#include "nsUnicodeBlock.h"
#include "nsDebug.h"
#include "nscore.h"

class nsUnicodeFontMappingCache;
class nsString;

 //--------------------------------------------------------------------------
typedef enum {
 kSerif  = 0,
 kSansSerif,
 kMonospace,
 kCursive,
 kFantasy, 
 kUknownGenericFontName
} nsGenericFontNameType;
//--------------------------------------------------------------------------

class nsUnicodeMappingUtil {
public:
	nsUnicodeMappingUtil();
	~nsUnicodeMappingUtil();
	void Init();
	void CleanUp();
	void Reset();
	inline PRBool ScriptEnabled(ScriptCode script) 
		{ return (0 != (mScriptEnabled & (1L << script))); };
	inline ScriptCode BlockToScript(nsUnicodeBlock blockID) 
		{ 
			NS_PRECONDITION(blockID < kUnicodeBlockSize, "illegal value");
			return (ScriptCode) mBlockToScriptMapping[blockID]; 
		};
	inline short ScriptFont(ScriptCode script) 
		{ 
			NS_PRECONDITION(script < smPseudoTotalScripts, "bad script code");
			return  mScriptFontMapping[script]; 
		};		
	nsGenericFontNameType MapGenericFontNameType(const nsString& aGenericName);
	inline nsString* GenericFontNameForScript(ScriptCode aScript, nsGenericFontNameType aType) const 
	{
			NS_PRECONDITION(aScript < smPseudoTotalScripts, "bad script code");
			NS_PRECONDITION(aType <= kUknownGenericFontName, "illegal value");
			if( aType >= kUknownGenericFontName)
				return nsnull;
			else
				return mGenericFontMapping[aScript][aType]; 
	}
    inline nsUnicodeFontMappingCache* GetFontMappingCache() { return mCache; };
	
  ScriptCode MapLangGroupToScriptCode(const char* aLangGroup);
	static nsUnicodeMappingUtil* GetSingleton();
	nsString *mGenericFontMapping[smPseudoTotalScripts][kUknownGenericFontName];
	
protected:
	void InitScriptEnabled();
    void InitGenericFontMapping();
    void InitBlockToScriptMapping();
    void InitScriptFontMapping();
    void InitFromPref();
   
private:
	PRUint32 mScriptEnabled;
	short 	 mScriptFontMapping[smPseudoTotalScripts];
	PRInt8   mBlockToScriptMapping[kUnicodeBlockSize];
	nsUnicodeFontMappingCache*	mCache;
	
	static nsUnicodeMappingUtil* gSingleton;

};
