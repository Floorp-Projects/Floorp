/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXNAMESPACEMAP_H
#define TRANSFRMX_TXNAMESPACEMAP_H

#include "nsIAtom.h"
#include "nsCOMArray.h"
#include "nsTArray.h"

class txNamespaceMap
{
public:
    txNamespaceMap();
    txNamespaceMap(const txNamespaceMap& aOther);

    nsrefcnt AddRef()
    {
        return ++mRefCnt;
    }
    nsrefcnt Release()
    {
        if (--mRefCnt == 0) {
            mRefCnt = 1; //stabilize
            delete this;
            return 0;
        }
        return mRefCnt;
    }

    nsresult mapNamespace(nsIAtom* aPrefix, const nsAString& aNamespaceURI);
    int32_t lookupNamespace(nsIAtom* aPrefix);
    int32_t lookupNamespaceWithDefault(const nsAString& aPrefix);

private:
    nsAutoRefCnt mRefCnt;
    nsCOMArray<nsIAtom> mPrefixes;
    nsTArray<int32_t> mNamespaces;
};

#endif //TRANSFRMX_TXNAMESPACEMAP_H
