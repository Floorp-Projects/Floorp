/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2002 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsLDAPBERValue.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsReadableUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPBERValue, nsILDAPBERValue)

nsLDAPBERValue::nsLDAPBERValue() : mValue(0), mSize(0)
{
}

nsLDAPBERValue::~nsLDAPBERValue()
{
    if (mValue) {
        nsMemory::Free(mValue);
    }
}

// void get (out unsigned long aCount, 
//           [array, size_is (aCount), retval] out octet aRetVal); */
NS_IMETHODIMP 
nsLDAPBERValue::Get(PRUint32 *aCount, PRUint8 **aRetVal)
{
    PRUint8 *array;

    // if mSize = 0, return a count of a 0 and a null pointer

    if (mSize) {
        // get a buffer to hold a copy of the data
        //
        array = NS_STATIC_CAST(PRUint8 *, nsMemory::Alloc(mSize));
        if (!array) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    
        // copy and return
        //
        memcpy(array, mValue, mSize);
    }

    *aCount = mSize;
    *aRetVal = mSize ? array : 0;
    return NS_OK;
}

// void set(in unsigned long aCount, 
//          [array, size_is(aCount)] in octet aValue);
NS_IMETHODIMP
nsLDAPBERValue::Set(PRUint32 aCount, PRUint8 *aValue)
{
    // get rid of any old value being held here
    //
    if (mValue) {
        nsMemory::Free(mValue);
    }

    // if this is a non-zero value, allocate a buffer and copy
    //
    if (aCount) { 
        // get a buffer to hold a copy of this data
        //
        mValue = NS_STATIC_CAST(PRUint8 *, nsMemory::Alloc(aCount));
        if (!mValue) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        // copy the data and return
        //
        memcpy(mValue, aValue, aCount);
    } else {
        // otherwise just set it to null
        //
        mValue = 0;
    }

    mSize = aCount;
    return NS_OK;
}

// void setFromUTF8(in AUTF8String aValue);
//
NS_IMETHODIMP
nsLDAPBERValue::SetFromUTF8(const nsACString & aValue)
{
    // get rid of any old value being held here
    //
    if (mValue) {
        nsMemory::Free(mValue);
    }

    // copy the data and return
    //
    // XXXdmose should really be ToNewUTF8String, once the appropriate 
    // signature for that exists
    //
    mSize = aValue.Length();
    if (mSize) {
        mValue = NS_REINTERPRET_CAST(PRUint8 *, ToNewCString(aValue)); 
    } else {
        mValue = 0;
    }
    return NS_OK;
}
