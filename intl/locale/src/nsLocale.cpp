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

#include "nsString.h"
#include "pratom.h"
#include "prtypes.h"
#include "nsISupports.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"

#define LOCALE_HASH_SIZE	0xFF


NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);
NS_DEFINE_IID(kLocaleCID, NS_LOCALE_CID);

/* nsILocale */
NS_IMPL_ISUPPORTS(nsLocale,kILocaleIID)


nsLocale::nsLocale(nsString** catagoryList,nsString** valueList, PRUint32 count)
:	fHashtable(NULL),
	fCatagoryCount(0)
{

	PRUint32	i;
	nsString*	key, *value;

  NS_INIT_REFCNT();

	fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
		&nsLocale::Hash_CompareNSString,&nsLocale::Hash_CompareNSString,NULL,NULL);
	NS_ASSERTION(fHashtable!=NULL,"nsLocale: failed to allocate PR_Hashtable");

	if (fHashtable!=NULL)
	{
		for(i=0;i<count;i++) 
		{
			key = new nsString(*catagoryList[i]);
			NS_ASSERTION(key!=NULL,"nsLocale: failed to allocate internal hash key");
			value = new nsString(*valueList[i]);
			NS_ASSERTION(value!=NULL,"nsLocale: failed to allocate internal hash value");
			(void)PL_HashTableAdd(fHashtable,key,value);
		}
	}
	
}

nsLocale::~nsLocale(void)
{	

	// enumerate all the entries with a delete function to
	// safely delete all the keys and values
	PL_HashTableEnumerateEntries(fHashtable,&nsLocale::Hash_EnmerateDelete,NULL);

	PL_HashTableDestroy(fHashtable);
		
}

NS_IMETHODIMP
nsLocale::GetCatagory(const nsString* catagory,nsString* result)
{

	const nsString*	value;

	value = (const nsString*)PL_HashTableLookup(fHashtable,catagory);
	if (value!=NULL)
	{
		(*result)=(*value);
		return NS_OK;
	}

	return NS_ERROR_FAILURE;

}


PLHashNumber
nsLocale::Hash_HashFunction(const void* key)
{
	const nsString*		stringKey;
	PLHashNumber		hash;
	PRInt32				length;

	stringKey	= (const nsString*)key;

	hash = (PLHashNumber)0;
	length = stringKey->Length();

	for(length-=1;length>=0;length--)
		hash += (PLHashNumber)((*stringKey)[length]);

	return hash;
}


PRIntn
nsLocale::Hash_CompareNSString(const void* s1, const void* s2)
{
	const nsString* string1;
	const nsString* string2;

	string1 = (const nsString*)s1;
	string2 = (const nsString*)s2;

	return string1->Equals(*string2);
}


PRIntn
nsLocale::Hash_EnmerateDelete(PLHashEntry *he, PRIntn index, void *arg)
{
	nsString*	key, *value;

	key = (nsString*)he->key;
	value = (nsString*)he->value;

	// delete the keys
	delete key;
	delete value;
	

	return (HT_ENUMERATE_NEXT | HT_ENUMERATE_REMOVE);
}
