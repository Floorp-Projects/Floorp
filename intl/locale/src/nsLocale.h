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
#ifndef nsLocale_h__
#define nsLocale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsILocale.h"

#include "plhash.h"

class nsLocale : public nsILocale {
	NS_DECL_ISUPPORTS

public:
	
	nsLocale(nsString** catagoryList,nsString** valueList, PRUint32 count);
	virtual ~nsLocale(void);
	
	NS_IMETHOD GetCatagory(const nsString* catagory, nsString* result);

private:
	
	static PLHashNumber Hash_HashFunction(const void* key);
	static PRIntn Hash_CompareNSString(const void* s1, const void* s2);
	static PRIntn Hash_EnmerateDelete(PLHashEntry *he, PRIntn index, void *arg);

	PLHashTable*	fHashtable;
	PRUint32		fCatagoryCount;

};


#endif
