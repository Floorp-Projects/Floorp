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
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef PROPERTYLIST_H
#define PROPERTYLIST_H

// A simple array class for managing name/value pairs typically fed to controls
// during initialization by IPersistPropertyBag

class PropertyList
{
    struct Property {
        BSTR    bstrName;
        VARIANT vValue;
    } *mProperties;
    unsigned long mListSize;
    unsigned long mMaxListSize;

    bool EnsureMoreSpace()
    {
        // Ensure enough space exists to accomodate a new item
        const unsigned long kGrowBy = 10;
        if (!mProperties)
        {
            mProperties = (Property *) malloc(sizeof(Property) * kGrowBy);
            if (!mProperties)
                return false;
            mMaxListSize = kGrowBy;
        }
        else if (mListSize == mMaxListSize)
        {
            Property *pNewProperties;
            pNewProperties = (Property *) realloc(mProperties, sizeof(Property) * (mMaxListSize + kGrowBy));
            if (!pNewProperties)
                return false;
            mProperties = pNewProperties;
            mMaxListSize += kGrowBy;
        }
        return true;
    }

public:
    PropertyList() :
      mProperties(NULL),
      mListSize(0),
      mMaxListSize(0)
    {
    }
    ~PropertyList()
    {
    }
    void Clear()
    {
        if (mProperties)
        {
            for (unsigned long i = 0; i < mListSize; i++)
            {
                SysFreeString(mProperties[i].bstrName); // Safe even if NULL
                VariantClear(&mProperties[i].vValue);
            }
            free(mProperties);
            mProperties = NULL;
        }
        mListSize = 0;
        mMaxListSize = 0;
    }
    unsigned long GetSize() const
    {
        return mListSize;
    }
    const BSTR GetNameOf(unsigned long nIndex) const
    {
        if (nIndex > mListSize)
        {
            return NULL;
        }
        return mProperties[nIndex].bstrName;
    }
    const VARIANT *GetValueOf(unsigned long nIndex) const
    {
        if (nIndex > mListSize)
        {
            return NULL;
        }
        return &mProperties[nIndex].vValue;
    }
    bool AddOrReplaceNamedProperty(const BSTR bstrName, const VARIANT &vValue)
    {
        if (!bstrName)
            return false;
        for (unsigned long i = 0; i < GetSize(); i++)
        {
            // Case insensitive
            if (wcsicmp(mProperties[i].bstrName, bstrName) == 0)
            {
                return SUCCEEDED(
                    VariantCopy(&mProperties[i].vValue, const_cast<VARIANT *>(&vValue)));
            }
        }
        return AddNamedProperty(bstrName, vValue);
    }
    bool AddNamedProperty(const BSTR bstrName, const VARIANT &vValue)
    {
        if (!bstrName || !EnsureMoreSpace())
            return false;
        Property *pProp = &mProperties[mListSize];
        pProp->bstrName = ::SysAllocString(bstrName);
        if (!pProp->bstrName)
        {
            return false;
        }
        VariantInit(&pProp->vValue);
        if (FAILED(VariantCopy(&pProp->vValue, const_cast<VARIANT *>(&vValue))))
        {
            SysFreeString(pProp->bstrName);
            return false;
        }
        mListSize++;
        return true;
    }
};

#endif