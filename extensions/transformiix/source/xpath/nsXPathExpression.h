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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com> (original author)
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

#ifndef nsXPathExpression_h__
#define nsXPathExpression_h__

#include "nsIDOMXPathExpression.h"
#include "txIXPathContext.h"
#include "txResultRecycler.h"
#include "nsAutoPtr.h"

class Expr;

/**
 * A class for evaluating an XPath expression string
 */
class nsXPathExpression : public nsIDOMXPathExpression
{
public:
    nsXPathExpression(nsAutoPtr<Expr>& aExpression,
                      txResultRecycler* aRecycler);
    virtual ~nsXPathExpression();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMXPathExpression interface
    NS_DECL_NSIDOMXPATHEXPRESSION

private:
    nsAutoPtr<Expr> mExpression;
    nsRefPtr<txResultRecycler> mRecycler;

    class EvalContextImpl : public txIEvalContext
    {
    public:
        EvalContextImpl(Node* aContextNode, txResultRecycler* aRecycler)
            : mContextNode(aContextNode),
              mLastError(NS_OK),
              mRecycler(aRecycler)
        {
        }

        ~EvalContextImpl()
        {
        }

        nsresult getError()
        {
            return mLastError;
        }

        TX_DECL_EVAL_CONTEXT;

    private:
        Node* mContextNode;
        nsresult mLastError;
        nsRefPtr<txResultRecycler> mRecycler;
    };
};

#endif
