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
	
	inline PRBool ScriptEnabled(ScriptCode script) 
		{ return (0 != (mScriptEnabled & (1L << script))); };
	inline ScriptCode BlockToScript(nsUnicodeBlock blockID) 
		{ 
			NS_PRECONDITION(blockID < kUnicodeBlockSize, "illegal value");
			return (ScriptCode) mBlockToScriptMapping[blockID]; 
		};
	inline short ScriptFont(ScriptCode script) 
		{ 
			NS_PRECONDITION(script < 32, "bad script code");
			return  mScriptFontMapping[script]; 
		};		
	nsGenericFontNameType MapGenericFontNameType(const nsString& aGenericName);
	inline nsString* GenericFontNameForScript(ScriptCode aScript, nsGenericFontNameType aType) const 
	{
			NS_PRECONDITION(aScript < 32, "bad script code");
			NS_PRECONDITION(aType <= kUknownGenericFontName, "illegal value");
			if( aType >= kUknownGenericFontName)
				return nsnull;
			else
				return mGenericFontMapping[aScript][aType]; 
	}
    inline nsUnicodeFontMappingCache* GetFontMappingCache() { return gCache; };
	
	static nsUnicodeMappingUtil* GetSingleton();
	
protected:
	void InitScriptEnabled();
    void InitGenericFontMapping();
    void InitBlockToScriptMapping();
    void InitScriptFontMapping();
    
private:
	PRUint32 mScriptEnabled;
	nsString *mGenericFontMapping[32][5];
	short 	 mScriptFontMapping[32];
	PRInt8   mBlockToScriptMapping[kUnicodeBlockSize];
	nsUnicodeFontMappingCache*	gCache;
	
	static nsUnicodeMappingUtil* gSingleton;

};
