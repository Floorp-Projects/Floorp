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

#ifndef nsUnicodeFontMappingMac_h__
#define nsUnicodeFontMappingMac_h__

#include "nsUnicodeBlock.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
 
class nsUnicodeMappingUtil;
class nsUnicodeFontMappingCache;

class nsUnicodeFontMappingMac {
public:
   nsUnicodeFontMappingMac(nsFont* aFont, nsIDeviceContext *aDeviceContext, 
   		const nsString& aDocumentCharset, const nsString& aLANG);
   short GetFontID(PRUnichar aChar);
   inline const short *GetScriptFallbackFonts() {
   		return mScriptFallbackFontIDs;
   }
   PRBool Equals(const nsUnicodeFontMappingMac& anther);
   
   static nsUnicodeFontMappingMac* GetCachedInstance(nsFont* aFont, nsIDeviceContext *aDeviceContext, 
   			const nsString& aDocumentCharset, const nsString& aLANG);
   
   
protected:
   PRBool ScriptMapInitComplete();
   void InitByFontFamily(nsFont* aFont, nsIDeviceContext *aDeviceContext);
   void InitByLANG(const nsString& aLANG);
   void InitByDocumentCharset(const nsString& aDocumentCharset);
   void InitDefaultScriptFonts();
   void processOneLangRegion(const char* aLanguage, const char* aRegion );
   nsUnicodeBlock GetBlock(PRUnichar aChar);
private:
   
   PRInt8 mPrivBlockToScript [kUnicodeBlockVarScriptMax] ;
   short  mScriptFallbackFontIDs [32] ;
   static nsUnicodeMappingUtil* gUtil;
   static nsUnicodeFontMappingCache* gCache;
};

#endif /* nsUnicodeFontMappingMac_h__ */
