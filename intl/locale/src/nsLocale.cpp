/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsString.h"
#include "pratom.h"
#include "prtypes.h"
#include "nsISupports.h"
#include "nsILocale.h"
#include "nsLocale.h"
#include "nsLocaleCID.h"
#include "nsCOMPtr.h"

#define LOCALE_HASH_SIZE	0xFF


/* nsILocale */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocale, nsILocale)

nsLocale::nsLocale(void)
:	fHashtable(nsnull), fCategoryCount(0)
{
	NS_INIT_REFCNT();

	fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
		&nsLocale::Hash_CompareNSString,&nsLocale::Hash_CompareNSString,NULL,NULL);
	NS_ASSERTION(fHashtable!=NULL,"nsLocale: failed to allocate PR_Hashtable");
}

nsLocale::nsLocale(nsLocale* other)
:	fHashtable(nsnull), fCategoryCount(0)
{
	NS_INIT_REFCNT();

	fHashtable = PL_NewHashTable(LOCALE_HASH_SIZE,&nsLocale::Hash_HashFunction,
		&nsLocale::Hash_CompareNSString,&nsLocale::Hash_CompareNSString,NULL,NULL);
	NS_ASSERTION(fHashtable!=NULL,"nsLocale: failed to allocate PR_Hashtable");

	//
	// enumerate Hash and copy
	//
	PL_HashTableEnumerateEntries(other->fHashtable,&nsLocale::Hash_EnumerateCopy,fHashtable);
}


nsLocale::nsLocale(nsString** categoryList,nsString** valueList, PRUint32 count)
:	fHashtable(NULL),
	fCategoryCount(0)
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
			key = new nsString(*categoryList[i]);
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
nsLocale::GetCategory(const nsString* category,nsString* result)
{

	const nsString*	value;

	value = (const nsString*)PL_HashTableLookup(fHashtable,category);
	if (value!=NULL)
	{
		(*result)=(*value);
		return NS_OK;
	}

	return NS_ERROR_FAILURE;

}


NS_IMETHODIMP
nsLocale::GetCategory(const PRUnichar *category,PRUnichar **result)
{

	nsString aCategory(category);
	const nsString*	value;

	value = (const nsString*)PL_HashTableLookup(fHashtable,&aCategory);
	if (value!=NULL)
	{
		(*result)=value->ToNewUnicode();
		return NS_OK;
	}

	return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
nsLocale::AddCategory(const PRUnichar *category, const PRUnichar *value)
{
	nsString* new_key = new nsString(category);
	if (!new_key) return NS_ERROR_OUT_OF_MEMORY;
	
	nsString* new_value = new nsString(value);
	if (!new_value) return NS_ERROR_OUT_OF_MEMORY;

	(void)PL_HashTableAdd(fHashtable,new_key,new_value);

	return NS_OK;
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
		hash += (PLHashNumber)stringKey->CharAt(length);

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
nsLocale::Hash_EnmerateDelete(PLHashEntry *he, PRIntn hashIndex, void *arg)
{
	nsString*	key, *value;

	key = (nsString*)he->key;
	value = (nsString*)he->value;

	// delete the keys
	delete key;
	delete value;
	

	return (HT_ENUMERATE_NEXT | HT_ENUMERATE_REMOVE);
}

PRIntn
nsLocale::Hash_EnumerateCopy(PLHashEntry *he, PRIntn hashIndex, void* arg)
{
	nsString	*new_key, *new_value;

	new_key = new nsString(*((nsString*)he->key));
	if (!new_key)
		return HT_ENUMERATE_STOP;

	new_value = new nsString(*((nsString*)he->value));
	if (!new_value)
		return HT_ENUMERATE_STOP;

	(void)PL_HashTableAdd((PLHashTable*)arg,new_key,new_value);

	return (HT_ENUMERATE_NEXT);
}





