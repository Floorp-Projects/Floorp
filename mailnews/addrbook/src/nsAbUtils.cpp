/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
 * Contributor(s): Csaba Borbola <csaba.borbola@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAbUtils.h"
#include "nsCRT.h"
#include "nsString.h"

/*
 * Convert the nsCStringArray to a char* array
 */
nsresult CStringArrayToCharPtrArray::Convert (nsCStringArray& array,
    PRUint32* returnPropertiesSize,
    char*** returnPropertiesArray,
    PRBool copyElements)
{
    PRUint32 size = array.Count ();
    if (size == 0)
        return NS_ERROR_FAILURE;

    if (!returnPropertiesSize || !returnPropertiesArray)
        return NS_ERROR_NULL_POINTER;

    *returnPropertiesSize = size;
    *returnPropertiesArray = NS_STATIC_CAST(char**, nsMemory::Alloc (sizeof (char* ) * size));
    if (!(*returnPropertiesArray))
        return NS_ERROR_OUT_OF_MEMORY;

    for (PRUint32 i = 0; i < size; i++)
    {

        if (copyElements == PR_TRUE)
            (*returnPropertiesArray)[i] =
                array[i]->ToNewCString();
        else
            (*returnPropertiesArray)[i] = NS_CONST_CAST(char *, (array[i]->ToNewCString()));
    }

    return NS_OK;
}

/*
 * Convert the char* array to nsCStringArray
 */
nsresult CharPtrArrayToCStringArray::Convert (nsCStringArray& returnPropertiesArray,
        PRUint32 arraySize,
        const char** array)
{
    if (!array)
        return NS_ERROR_NULL_POINTER;
    
    if (!arraySize)
        return NS_OK;

    returnPropertiesArray.Clear ();

    for (PRUint32 i = 0; i < arraySize; i++)
        returnPropertiesArray.AppendCString (nsCAutoString (array[i]));

    return NS_OK;
}

/*
 * Convert the PRUnichar* array to nsStringArray
 */
nsresult StringArrayToPRUnicharPtrArray::Convert (nsStringArray& array,
    PRUint32* returnPropertiesSize,
    PRUnichar*** returnPropertiesArray,
    PRBool copyElements)
{
    PRUint32 size = array.Count ();
    if (size == 0)
        return NS_ERROR_FAILURE;

    if (!returnPropertiesSize || !returnPropertiesArray)
        return NS_ERROR_NULL_POINTER;

    *returnPropertiesSize = size;
    *returnPropertiesArray = NS_STATIC_CAST(PRUnichar**, nsMemory::Alloc (sizeof (PRUnichar* ) * size));
    if (!(*returnPropertiesArray))
        return NS_ERROR_OUT_OF_MEMORY;

    for (PRUint32 i = 0; i < size; i++)
    {

        if (copyElements == PR_TRUE)
            (*returnPropertiesArray)[i] =
                array[i]->ToNewUnicode();
        else
            (*returnPropertiesArray)[i] = NS_REINTERPRET_CAST(PRUnichar*,array[i]->ToNewCString());
    }

    return NS_OK;
}

/*
 * Convert the PRUnichar* array to nsStringArray
 */
nsresult PRUnicharPtrArrayToStringArray::Convert (nsStringArray& returnPropertiesArray,
    PRUint32 arraySize,
    const PRUnichar** array)
{
    if (!array)
        return NS_ERROR_NULL_POINTER;
    
    if (!arraySize)
        return NS_OK;

    returnPropertiesArray.Clear ();

    for (PRUint32 i = 0; i < arraySize; i++)
        returnPropertiesArray.AppendString (nsAutoString (array[i]));

    return NS_OK;
}

/*
 * Convert array of keys and values to nsHashtable
 */
nsresult PropertyPtrArraysToHashtable::Convert (
        nsHashtable& propertySet,
        PRUint32 propertiesSize,
        const char** propertyNameArray,
        const PRUnichar** propertyValueArray)
{
    if (!propertyNameArray || !propertyValueArray)
        return NS_ERROR_NULL_POINTER;

    if (!propertiesSize)
        return NS_OK;

    propertySet.Reset ();

    for (PRUint32 i = 0; i < propertiesSize; i++)
    {
        nsCStringKey key (propertyNameArray[i], -1, nsCStringKey::NEVER_OWN);
        propertySet.Put (&key, NS_REINTERPRET_CAST(void *, NS_CONST_CAST(PRUnichar *, propertyValueArray[i])));
    }

    return NS_OK;
}


/*
 * nsHashtable enumerator callback data
 *
 * Contains the key and value arrays and
 * the current position a new entry may
 * be added
 */
struct closureStruct
{
    PRUint32 position;
    char** propertyNameArray;
    PRUnichar** propertyValueArray;
};

/*
 * nsHashtable enumeration callback procedure
 *
 * References values from the hashtable entry
 * into the associated position of the key and
 * value arrays
 */
PR_STATIC_CALLBACK(PRBool) enumerateEntries(nsHashKey *aKey, void *aData, void* closure)
{
    closureStruct* s  = NS_REINTERPRET_CAST(closureStruct*, closure);
    nsCStringKey* key = NS_REINTERPRET_CAST(nsCStringKey*, aKey);

    s->propertyNameArray[s->position] =  NS_CONST_CAST(char* ,key->GetString ());
    s->propertyValueArray[s->position] = NS_STATIC_CAST(PRUnichar *, aData);
    s->position++;


    return PR_TRUE;
}

/*
 * Convert nsHashtable to array of keys and values
 */
nsresult HashtableToPropertyPtrArrays::Convert (
        nsHashtable& propertySet,
        PRUint32* propertiesSize,
        char*** propertyNameArray,
        PRUnichar*** propertyValueArray)
{
    if (!propertyNameArray || !propertyValueArray || !propertiesSize)
        return NS_ERROR_NULL_POINTER;

    *propertiesSize = propertySet.Count ();
    if (*propertiesSize == 0)
        return NS_OK;


    *propertyNameArray = 
        NS_STATIC_CAST(char**, nsMemory::Alloc (sizeof (char* ) * (*propertiesSize)));
    if (!(*propertyNameArray))
        return NS_ERROR_OUT_OF_MEMORY;
    *propertyValueArray = 
        NS_STATIC_CAST(PRUnichar**, nsMemory::Alloc (sizeof (PRUnichar* ) * (*propertiesSize)));
    if (!(*propertyValueArray))
    {
        nsMemory::Free (*propertyNameArray);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Set up enumerator callback structure
    closureStruct s;
    s.position = 0;
    s.propertyNameArray = *propertyNameArray;
    s.propertyValueArray = *propertyValueArray;
    // Enumerate over hashtable entries
    propertySet.Enumerate (enumerateEntries, NS_STATIC_CAST(void* ,&s));

    return NS_OK;
}

