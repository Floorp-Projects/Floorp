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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
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

#include "nsResourceSet.h"

nsResourceSet::nsResourceSet(const nsResourceSet& aResourceSet)
    : mResources(nsnull),
      mCount(0),
      mCapacity(0)
{
    ConstIterator last = aResourceSet.Last();
    for (ConstIterator resource = aResourceSet.First(); resource != last; ++resource)
        Add(*resource);
}


nsResourceSet&
nsResourceSet::operator=(const nsResourceSet& aResourceSet)
{
    Clear();
    ConstIterator last = aResourceSet.Last();
    for (ConstIterator resource = aResourceSet.First(); resource != last; ++resource)
        Add(*resource);
    return *this;
}

nsResourceSet::~nsResourceSet()
{
    MOZ_COUNT_DTOR(nsResourceSet);
    Clear();
    delete[] mResources;
}

nsresult
nsResourceSet::Clear()
{
    while (--mCount >= 0) {
        NS_RELEASE(mResources[mCount]);
    }
    mCount = 0;
    return NS_OK;
}

nsresult
nsResourceSet::Add(nsIRDFResource* aResource)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    if (Contains(aResource))
        return NS_OK;

    if (mCount >= mCapacity) {
        PRInt32 capacity = mCapacity + 4;
        nsIRDFResource** resources = new nsIRDFResource*[capacity];
        if (! resources)
            return NS_ERROR_OUT_OF_MEMORY;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            resources[i] = mResources[i];

        delete[] mResources;

        mResources = resources;
        mCapacity = capacity;
    }

    mResources[mCount++] = aResource;
    NS_ADDREF(aResource);
    return NS_OK;
}

void
nsResourceSet::Remove(nsIRDFResource* aProperty)
{
    PRBool found = PR_FALSE;

    nsIRDFResource** res = mResources;
    nsIRDFResource** limit = mResources + mCount;
    while (res < limit) {
        if (found) {
            *(res - 1) = *res;
        }
        else if (*res == aProperty) {
            NS_RELEASE(*res);
            found = PR_TRUE;
        }
        ++res;
    }

    if (found)
        --mCount;
}

PRBool
nsResourceSet::Contains(nsIRDFResource* aResource) const
{
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        if (mResources[i] == aResource)
            return PR_TRUE;
    }

    return PR_FALSE;
}

