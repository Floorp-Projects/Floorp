/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txXPathObjectAdaptor_h__
#define txXPathObjectAdaptor_h__

#include "txExprResult.h"
#include "txINodeSet.h"
#include "txIXPathObject.h"

/**
 * Implements an XPCOM wrapper around XPath data types boolean, number, string,
 * or nodeset.
 */

class txXPathObjectAdaptor : public txIXPathObject
{
public:
    explicit txXPathObjectAdaptor(txAExprResult* aValue) : mValue(aValue)
    {
        NS_ASSERTION(aValue,
                     "Don't create a txXPathObjectAdaptor if you don't have a "
                     "txAExprResult");
    }

    NS_DECL_ISUPPORTS

    NS_IMETHOD_(txAExprResult*) GetResult() override
    {
        return mValue;
    }

protected:
    txXPathObjectAdaptor() : mValue(nullptr)
    {
    }

    virtual ~txXPathObjectAdaptor() {}

    RefPtr<txAExprResult> mValue;
};

#endif // txXPathObjectAdaptor_h__
