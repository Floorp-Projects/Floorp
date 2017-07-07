/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsResourceSet_h__
#define nsResourceSet_h__

#include "nsIRDFResource.h"

class nsResourceSet
{
public:
    nsResourceSet()
        : mResources(nullptr),
          mCount(0),
          mCapacity(0) {
        MOZ_COUNT_CTOR(nsResourceSet); }

    nsResourceSet(const nsResourceSet& aResourceSet);

    nsResourceSet& operator=(const nsResourceSet& aResourceSet);

    ~nsResourceSet();

    nsresult Clear();
    nsresult Add(nsIRDFResource* aProperty);
    void Remove(nsIRDFResource* aProperty);

    bool Contains(nsIRDFResource* aProperty) const;

protected:
    nsIRDFResource** mResources;
    int32_t mCount;
    int32_t mCapacity;

public:
    class ConstIterator {
    protected:
        nsIRDFResource** mCurrent;

    public:
        ConstIterator() : mCurrent(nullptr) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            ++mCurrent;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            ++mCurrent;
            return result; }

        /*const*/ nsIRDFResource* operator*() const {
            return *mCurrent; }

        /*const*/ nsIRDFResource* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
            return *mCurrent; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

    protected:
        explicit ConstIterator(nsIRDFResource** aProperty) : mCurrent(aProperty) {}
        friend class nsResourceSet;
    };

    ConstIterator First() const { return ConstIterator(mResources); }
    ConstIterator Last() const { return ConstIterator(mResources + mCount); }
};

#endif // nsResourceSet_h__

