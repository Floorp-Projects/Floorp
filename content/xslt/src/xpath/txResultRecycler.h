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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

#ifndef txResultRecycler_h__
#define txResultRecycler_h__

#include "nsCOMPtr.h"
#include "txStack.h"

class txAExprResult;
class StringResult;
class txNodeSet;
class txXPathNode;
class NumberResult;
class BooleanResult;

class txResultRecycler
{
public:
    txResultRecycler();
    ~txResultRecycler();
    nsresult init();

    void AddRef()
    {
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "txResultRecycler", sizeof(*this));
    }
    void Release()
    {
        --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, "txResultRecycler");
        if (mRefCnt == 0) {
            mRefCnt = 1; //stabilize
            delete this;
        }
    }

    /**
     * Returns an txAExprResult to this recycler for reuse.
     * @param aResult  result to recycle
     */
    void recycle(txAExprResult* aResult);

    /**
     * Functions to return results that will be fully used by the caller.
     * Returns nsnull on out-of-memory and an inited result otherwise.
     */
    nsresult getStringResult(StringResult** aResult);
    nsresult getStringResult(const nsAString& aValue, txAExprResult** aResult);
    nsresult getNodeSet(txNodeSet** aResult);
    nsresult getNodeSet(txNodeSet* aNodeSet, txNodeSet** aResult);
    nsresult getNodeSet(const txXPathNode& aNode, txAExprResult** aResult);
    nsresult getNumberResult(double aValue, txAExprResult** aResult);

    /**
     * Functions to return a txAExprResult that is shared across several
     * clients and must not be modified. Never returns nsnull.
     */
    void getEmptyStringResult(txAExprResult** aResult);
    void getBoolResult(bool aValue, txAExprResult** aResult);

    /**
     * Functions that return non-shared resultsobjects
     */
    nsresult getNonSharedNodeSet(txNodeSet* aNodeSet, txNodeSet** aResult);

private:
    nsAutoRefCnt mRefCnt;
    txStack mStringResults;
    txStack mNodeSetResults;
    txStack mNumberResults;
    StringResult* mEmptyStringResult;
    BooleanResult* mTrueResult;
    BooleanResult* mFalseResult;
};

#endif //txResultRecycler_h__
