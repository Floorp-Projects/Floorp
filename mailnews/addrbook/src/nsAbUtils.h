/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Csaba Borbola <csaba.borbola@sun.com>
 *   Paul Sandoz   <paul.sandoz@sun.com>
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

#ifndef nsAbUtils_h__
#define nsAbUtils_h__

#include "nsMemory.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"

/*
 * Wrapper class to automatically free an array of
 * char* when class goes out of scope
 */
class CharPtrArrayGuard
{
public:
    CharPtrArrayGuard (PRBool freeElements = PR_TRUE) :
        mFreeElements (freeElements),
        mArray (0),
        mSize (0)
    {
    }

    ~CharPtrArrayGuard ()
    {
        Free ();
    }

    char* operator[](int i)
    {
        return mArray[i];
    }

    PRUint32* GetSizeAddr(void)
    {
        return &mSize;
    }

    PRUint32 GetSize(void)
    {
        return mSize;
    }

    char*** GetArrayAddr(void)
    {
        return &mArray;
    }

    const char** GetArray(void)
    {
        return (const char** ) mArray;
    }

public:

private:
    PRBool mFreeElements;
    char **mArray;
    PRUint32 mSize;

    void Free ()
    {
        if (!mArray)
            return;

        if (mFreeElements)
            NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mSize, mArray);
        else
        {
          nsMemory::Free(mArray);
          mArray=nsnull;
        }
    }
};


/*
 * Helper class to convert an nsCStringArray
 * to an array of char*
 * By default strings will be copied, unless
 * the copyElements parameter is set to PR_FALSE.
 */
class CStringArrayToCharPtrArray
{
public:
    static nsresult Convert (nsCStringArray& array,
        PRUint32* returnPropertiesSize,
        char*** returnPropertiesArray,
        PRBool copyElements = PR_TRUE);
};

/*
 * Helper class to convert an array of char*
 * to an nsCStringArray
 */
class CharPtrArrayToCStringArray
{
public:
    static nsresult Convert (nsCStringArray& array,
        PRUint32 returnPropertiesSize,
        const char** returnPropertiesArray);
};


/*
 * Wrapper class to automatically free an array of
 * PRUnichar* when class goes out of scope
 */
class PRUnicharPtrArrayGuard
{
public:
    PRUnicharPtrArrayGuard (PRBool freeElements = PR_TRUE) :
        mFreeElements (freeElements),
        mArray (0),
        mSize (0)
    {
    }

    ~PRUnicharPtrArrayGuard ()
    {
        Free ();
    }

    PRUnichar* operator[](int i)
    {
        return mArray[i];
    }

    PRUint32* GetSizeAddr(void)
    {
        return &mSize;
    }

    PRUint32 GetSize(void)
    {
        return mSize;
    }

    PRUnichar*** GetArrayAddr(void)
    {
        return &mArray;
    }

    const PRUnichar** GetArray(void)
    {
        return (const PRUnichar** ) mArray;
    }

public:

private:
    PRBool mFreeElements;
    PRUnichar **mArray;
    PRUint32 mSize;
    void Free ()
    {
        if (!mArray)
            return;

        if (mFreeElements)
          NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mSize, mArray);
        else
        {
          nsMemory::Free(mArray);
          mArray=nsnull;
        }
    }
};

/*
 * Helper class to convert an nsStringArray
 * to an array of PRUnichar*
 * By default strings will be copied, unless
 * the copyElements parameter is set to PR_FALSE.
 */
class StringArrayToPRUnicharPtrArray
{
public:
    static nsresult Convert (nsStringArray& array,
        PRUint32* returnPropertiesSize,
        PRUnichar*** returnPropertiesArray,
        PRBool copyElements = PR_TRUE);
};

/*
 * Helper class to convert an array of PRUnichar*
 * to an nsStringArray
 */
class PRUnicharPtrArrayToStringArray
{
public:
    static nsresult Convert (nsStringArray& returnPropertiesArray,
        PRUint32 returnPropertiesSize,
        const PRUnichar** array);
};

#endif  /* nsAbUtils_h__ */
