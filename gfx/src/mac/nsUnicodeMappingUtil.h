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
#include <Script.h>

#include "prtypes.h"
#include "nsUnicodeBlock.h"
#include "nsDebug.h"
#include "nscore.h"
#include "nsIPref.h"

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
	
  ScriptCode MapLangGroupToScriptCode(const char* aLangGroup);
	static nsUnicodeMappingUtil* GetSingleton();
	static void FreeSingleton();
	nsString *mGenericFontMapping[smPseudoTotalScripts][kUknownGenericFontName];
	
protected:
    void InitGenericFontMapping();
    void InitBlockToScriptMapping();
    void InitScriptFontMapping();
    void InitFromPref();
   
  static int  PR_CALLBACK_DECL PrefChangedCallback( const char* aPrefName, void* instance_data);
  static void PR_CALLBACK_DECL PrefEnumCallback(const char* aName, void* aClosure);
    
private:
	short 	 mScriptFontMapping[smPseudoTotalScripts];
	PRInt8   mBlockToScriptMapping[kUnicodeBlockSize];
	nsCOMPtr<nsIPref> mPref;
	
	static nsUnicodeMappingUtil* gSingleton;

};
