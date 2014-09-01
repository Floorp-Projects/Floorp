/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txNodeSetAdaptor_h__
#define txNodeSetAdaptor_h__

#include "txINodeSet.h"
#include "txNodeSet.h"
#include "txXPathObjectAdaptor.h"

/**
 * Implements an XPCOM wrapper around an XPath NodeSet.
 */

class txNodeSetAdaptor : public txXPathObjectAdaptor,
                         public txINodeSet 
{
public:
    txNodeSetAdaptor();
    explicit txNodeSetAdaptor(txNodeSet* aNodeSet);

    nsresult Init();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_TXINODESET

protected:
    ~txNodeSetAdaptor() {}

private:
    txNodeSet* NodeSet()
    {
        return static_cast<txNodeSet*>(mValue.get());
    }

    bool mWritable;
};

#endif // txNodeSetAdaptor_h__
