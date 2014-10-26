/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsResourceSet.h"

nsResourceSet::nsResourceSet(const nsResourceSet& aResourceSet)
    : mResources(nullptr),
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
    NS_PRECONDITION(aResource != nullptr, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    if (Contains(aResource))
        return NS_OK;

    if (mCount >= mCapacity) {
        int32_t capacity = mCapacity + 4;
        nsIRDFResource** resources = new nsIRDFResource*[capacity];
        if (! resources)
            return NS_ERROR_OUT_OF_MEMORY;

        for (int32_t i = mCount - 1; i >= 0; --i)
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
    bool found = false;

    nsIRDFResource** res = mResources;
    nsIRDFResource** limit = mResources + mCount;
    while (res < limit) {
        if (found) {
            *(res - 1) = *res;
        }
        else if (*res == aProperty) {
            NS_RELEASE(*res);
            found = true;
        }
        ++res;
    }

    if (found)
        --mCount;
}

bool
nsResourceSet::Contains(nsIRDFResource* aResource) const
{
    for (int32_t i = mCount - 1; i >= 0; --i) {
        if (mResources[i] == aResource)
            return true;
    }

    return false;
}

