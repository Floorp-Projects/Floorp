/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef TRANSFRMX_VARIABLEMAP_H
#define TRANSFRMX_VARIABLEMAP_H

#include "txError.h"
#include "XMLUtils.h"
#include "ExprResult.h"
#include "txExpandedNameMap.h"

class txVariableMap {
public:
    txVariableMap();
    ~txVariableMap();
    
    nsresult bindVariable(const txExpandedName& aName, txAExprResult* aValue);

    void getVariable(const txExpandedName& aName, txAExprResult** aResult);
    
    void removeVariable(const txExpandedName& aName);

private:
    txExpandedNameMap mMap;
};


inline
txVariableMap::txVariableMap()
    : mMap(MB_FALSE)
{
}

inline
txVariableMap::~txVariableMap()
{
    txExpandedNameMap::iterator iter(mMap);
    while (iter.next()) {
        txAExprResult* res = NS_STATIC_CAST(txAExprResult*, iter.value());
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
    return rv;
}

inline void
txVariableMap::getVariable(const txExpandedName& aName, txAExprResult** aResult)
{
    *aResult = NS_STATIC_CAST(txAExprResult*, mMap.get(aName));
    if (*aResult) {
        NS_ADDREF(*aResult);
    }
}

inline void
txVariableMap::removeVariable(const txExpandedName& aName)
{
    txAExprResult* var = NS_STATIC_CAST(txAExprResult*, mMap.remove(aName));
    NS_IF_RELEASE(var);
}

#endif //TRANSFRMX_VARIABLEMAP_H
