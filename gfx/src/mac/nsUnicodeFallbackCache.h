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

#ifndef nsUnicodeFallbackCache_h__
#define nsUnicodeFallbackCache_h__
#include "prtypes.h"
#include "plhash.h"
#include <Script.h>
#include "nsDebug.h"
#include "nscore.h"

class nsUnicodeFallbackCache
{
public:
	nsUnicodeFallbackCache() 
	{
		mTable = PL_NewHashTable(8, (PLHashFunction)HashKey, 
								(PLHashComparator)CompareKeys, 
								(PLHashComparator)CompareValues,
								nsnull, nsnull);
		mCount = 0;
	};
	~nsUnicodeFallbackCache() 
	{
		if (mTable)
		{
			PL_HashTableEnumerateEntries(mTable, FreeHashEntries, 0);
			PL_HashTableDestroy(mTable);
			mTable = nsnull;
		}
	};

	inline PRBool	Get(PRUnichar aChar, ScriptCode& oScript) 
	{
		ScriptCode ret = (ScriptCode)PL_HashTableLookup(mTable, (void*)aChar);
		oScript = 0x00FF & ret ;
		return 0x00 != (0xFF00 & ret);
	};
	
	inline void	Set(PRUnichar aChar, ScriptCode aScript) 
	{
		PL_HashTableAdd(mTable,(void*) aChar, (void*)(aScript | 0xFF00));
		mCount ++;
	};

	inline static nsUnicodeFallbackCache* GetSingleton() 
	{
			if(! gSingleton)
				gSingleton = new nsUnicodeFallbackCache();
			return gSingleton;
	}
private:
	inline static PR_CALLBACK PLHashNumber HashKey(const void *aKey) 
	{
		return (PRUnichar)aKey;
	};
	
	inline static PR_CALLBACK PRIntn		CompareKeys(const void *v1, const void *v2) 
	{
		return  (((PRUnichar ) v1) == ((PRUnichar ) v2));
	};
	
	inline static PR_CALLBACK PRIntn		CompareValues(const void *v1, const void *v2) 
	{
		return (((ScriptCode)v1) == ((ScriptCode)v2));
	};
	inline static PR_CALLBACK PRIntn		FreeHashEntries(PLHashEntry *he, PRIntn italic, void *arg) 
	{
		return HT_ENUMERATE_REMOVE;
	};

	struct PLHashTable*		mTable;
	PRUint32				mCount;

	static nsUnicodeFallbackCache* gSingleton;
};
#endif nsUnicodeFallbackCache_h__
