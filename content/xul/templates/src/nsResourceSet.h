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
#ifndef nsResourceSet_h__
#define nsResourceSet_h__

#include "nsIRDFResource.h"

class nsResourceSet
{
public:
    nsResourceSet()
        : mResources(nsnull),
          mCount(0),
          mCapacity(0) {
        MOZ_COUNT_CTOR(nsResourceSet); }

    nsResourceSet(const nsResourceSet& aResourceSet);

    nsResourceSet& operator=(const nsResourceSet& aResourceSet);
    
    ~nsResourceSet();

    nsresult Clear();
    nsresult Add(nsIRDFResource* aProperty);

    PRBool Contains(nsIRDFResource* aProperty) const;

protected:
    nsIRDFResource** mResources;
    PRInt32 mCount;
    PRInt32 mCapacity;

public:
    class ConstIterator {
    protected:
        nsIRDFResource** mCurrent;

    public:
        ConstIterator() : mCurrent(nsnull) {}

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

        /*const*/ nsIRDFResource* operator->() const {
            return *mCurrent; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

    protected:
        ConstIterator(nsIRDFResource** aProperty) : mCurrent(aProperty) {}
        friend class nsResourceSet;
    };

    ConstIterator First() const { return ConstIterator(mResources); }
    ConstIterator Last() const { return ConstIterator(mResources + mCount); }
};

#endif // nsResourceSet_h__

