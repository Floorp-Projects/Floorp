/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

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

PRBool
nsResourceSet::Contains(nsIRDFResource* aResource) const
{
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        if (mResources[i] == aResource)
            return PR_TRUE;
    }

    return PR_FALSE;
}

