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
#ifndef nsLocale_h__
#define nsLocale_h__

#include "nsString.h"
#include "nsILocale.h"
#include "plhash.h"

class nsLocale : public nsILocale {
	friend class nsLocaleDefinition;
	friend class nsLocaleService;
	NS_DECL_ISUPPORTS

public:
	nsLocale(void);
	nsLocale(nsString** categoryList,nsString** valueList, PRUint32 count);
	nsLocale(nsLocale* other);
	virtual ~nsLocale(void);
	
	NS_IMETHOD GetCategory(const nsString* category, nsString* result);

  /* Declare methods from nsILocale */
  NS_DECL_NSILOCALE

protected:
	
	NS_IMETHOD AddCategory(const PRUnichar* category, const PRUnichar* value);

	static PLHashNumber Hash_HashFunction(const void* key);
	static PRIntn Hash_CompareNSString(const void* s1, const void* s2);
	static PRIntn Hash_EnmerateDelete(PLHashEntry *he, PRIntn hashIndex, void *arg);
	static PRIntn Hash_EnumerateCopy(PLHashEntry *he, PRIntn hashIndex, void *arg);

	PLHashTable*	fHashtable;
	PRUint32		fCategoryCount;

};


#endif
