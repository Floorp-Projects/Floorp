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
    void Remove(nsIRDFResource* aProperty);

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

