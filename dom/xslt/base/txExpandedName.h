/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_EXPANDEDNAME_H
#define TRANSFRMX_EXPANDEDNAME_H

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "mozilla/dom/NameSpaceConstants.h"

class txNamespaceMap;

class txExpandedName {
public:
    txExpandedName() : mNamespaceID(kNameSpaceID_None)
    {
    }

    txExpandedName(int32_t aNsID,
                   nsIAtom* aLocalName) : mNamespaceID(aNsID),
                                          mLocalName(aLocalName)
    {
    }

    txExpandedName(const txExpandedName& aOther) :
        mNamespaceID(aOther.mNamespaceID),
        mLocalName(aOther.mLocalName)
    {
    }

    nsresult init(const nsAString& aQName, txNamespaceMap* aResolver,
                  bool aUseDefault);

    void reset()
    {
        mNamespaceID = kNameSpaceID_None;
        mLocalName = nullptr;
    }

    bool isNull()
    {
        return mNamespaceID == kNameSpaceID_None && !mLocalName;
    }

    txExpandedName& operator = (const txExpandedName& rhs)
    {
        mNamespaceID = rhs.mNamespaceID;
        mLocalName = rhs.mLocalName;
        return *this;
    }

    bool operator == (const txExpandedName& rhs) const
    {
        return ((mLocalName == rhs.mLocalName) &&
                (mNamespaceID == rhs.mNamespaceID));
    }

    bool operator != (const txExpandedName& rhs) const
    {
        return ((mLocalName != rhs.mLocalName) ||
                (mNamespaceID != rhs.mNamespaceID));
    }

    int32_t mNamespaceID;
    nsCOMPtr<nsIAtom> mLocalName;
};

#endif
