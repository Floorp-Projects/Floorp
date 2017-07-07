/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_VARIABLEMAP_H
#define TRANSFRMX_VARIABLEMAP_H

#include "nsError.h"
#include "txXMLUtils.h"
#include "txExprResult.h"
#include "txExpandedNameMap.h"

class txVariableMap {
public:
    txVariableMap();
    ~txVariableMap();

    nsresult bindVariable(const txExpandedName& aName, txAExprResult* aValue);

    void getVariable(const txExpandedName& aName, txAExprResult** aResult);

    void removeVariable(const txExpandedName& aName);

private:
    txExpandedNameMap<txAExprResult> mMap;
};


inline
txVariableMap::txVariableMap()
{
    MOZ_COUNT_CTOR(txVariableMap);
}

inline
txVariableMap::~txVariableMap()
{
    MOZ_COUNT_DTOR(txVariableMap);

    txExpandedNameMap<txAExprResult>::iterator iter(mMap);
    while (iter.next()) {
        txAExprResult* res = iter.value();
        NS_RELEASE(res);
    }
}

inline nsresult
txVariableMap::bindVariable(const txExpandedName& aName, txAExprResult* aValue)
{
    NS_ASSERTION(aValue, "can't add null-variables to a txVariableMap");
    nsresult rv = mMap.add(aName, aValue);
    if (NS_SUCCEEDED(rv)) {
        NS_ADDREF(aValue);
    }
    else if (rv == NS_ERROR_XSLT_ALREADY_SET) {
        rv = NS_ERROR_XSLT_VAR_ALREADY_SET;
    }
    return rv;
}

inline void
txVariableMap::getVariable(const txExpandedName& aName, txAExprResult** aResult)
{
    *aResult = mMap.get(aName);
    if (*aResult) {
        NS_ADDREF(*aResult);
    }
}

inline void
txVariableMap::removeVariable(const txExpandedName& aName)
{
    txAExprResult* var = mMap.remove(aName);
    NS_IF_RELEASE(var);
}

#endif //TRANSFRMX_VARIABLEMAP_H
