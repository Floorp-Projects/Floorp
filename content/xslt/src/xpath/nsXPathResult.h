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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#ifndef nsXPathResult_h__
#define nsXPathResult_h__

#include "txExprResult.h"
#include "nsIDOMXPathResult.h"
#include "nsIDocument.h"
#include "nsStubMutationObserver.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"

// {15b9b301-2012-11d6-a7f2-e6d0a678995c}
#define NS_IXPATHRESULT_IID \
{ 0x15b9b301, 0x2012, 0x11d6, {0xa7, 0xf2, 0xe6, 0xd0, 0xa6, 0x78, 0x99, 0x5c }}

class nsIXPathResult : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPATHRESULT_IID)
    virtual nsresult SetExprResult(txAExprResult *aExprResult,
                                   PRUint16 aResultType) = 0;
    virtual nsresult GetExprResult(txAExprResult **aExprResult) = 0;
    virtual nsresult Clone(nsIXPathResult **aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPathResult, NS_IXPATHRESULT_IID)

/**
 * Helper class to keep Mozilla node objects alive as long as the nodeset is
 * alive.
 */
class txResultHolder
{
public:
    ~txResultHolder()
    {
      releaseNodeSet();
    }

    txAExprResult *get()
    {
        return mResult;
    }
    void set(txAExprResult *aResult);

private:
    void releaseNodeSet();

    nsRefPtr<txAExprResult> mResult;
};

/**
 * A class for evaluating an XPath expression string
 */
class nsXPathResult : public nsIDOMXPathResult,
                      public nsStubMutationObserver,
                      public nsIXPathResult
{
public:
    nsXPathResult();
    virtual ~nsXPathResult();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMXPathResult interface
    NS_DECL_NSIDOMXPATHRESULT

    // nsIMutationObserver interface
    NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

    // nsIXPathResult interface
    nsresult SetExprResult(txAExprResult *aExprResult, PRUint16 aResultType);
    nsresult GetExprResult(txAExprResult **aExprResult);
    nsresult Clone(nsIXPathResult **aResult);

private:
    PRBool isSnapshot() const
    {
        return mResultType == UNORDERED_NODE_SNAPSHOT_TYPE ||
               mResultType == ORDERED_NODE_SNAPSHOT_TYPE;
    }
    PRBool isIterator() const
    {
        return mResultType == UNORDERED_NODE_ITERATOR_TYPE ||
               mResultType == ORDERED_NODE_ITERATOR_TYPE;
    }
    PRBool isNode() const
    {
        return mResultType == FIRST_ORDERED_NODE_TYPE ||
               mResultType == ANY_UNORDERED_NODE_TYPE;
    }

    void Invalidate();

    txResultHolder mResult;
    nsCOMPtr<nsIDocument> mDocument;
    PRUint32 mCurrentPos;
    PRUint16 mResultType;
    PRPackedBool mInvalidIteratorState;
};

#endif
