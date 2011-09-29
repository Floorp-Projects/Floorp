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

#include "txExpr.h"

nsresult
txLiteralExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    NS_ENSURE_TRUE(mValue, NS_ERROR_OUT_OF_MEMORY);

    *aResult = mValue;
    NS_ADDREF(*aResult);

    return NS_OK;
}

static Expr::ResultType resultTypes[] =
{
    Expr::NODESET_RESULT, // NODESET
    Expr::BOOLEAN_RESULT, // BOOLEAN
    Expr::NUMBER_RESULT,  // NUMBER
    Expr::STRING_RESULT,  // STRING
    Expr::RTF_RESULT      // RESULT_TREE_FRAGMENT
};

Expr::ResultType
txLiteralExpr::getReturnType()
{
    return resultTypes[mValue->getResultType()];
}

Expr*
txLiteralExpr::getSubExprAt(PRUint32 aPos)
{
    return nsnull;
}
void
txLiteralExpr::setSubExprAt(PRUint32 aPos, Expr* aExpr)
{
    NS_NOTREACHED("setting bad subexpression index");
}

bool
txLiteralExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return PR_FALSE;
}

#ifdef TX_TO_STRING
void
txLiteralExpr::toString(nsAString& aStr)
{
    switch (mValue->getResultType()) {
        case txAExprResult::NODESET:
        {
            aStr.AppendLiteral(" { Nodeset literal } ");
            return;
        }
        case txAExprResult::BOOLEAN:
        {
            if (mValue->booleanValue()) {
              aStr.AppendLiteral("true()");
            }
            else {
              aStr.AppendLiteral("false()");
            }
            return;
        }
        case txAExprResult::NUMBER:
        {
            Double::toString(mValue->numberValue(), aStr);
            return;
        }
        case txAExprResult::STRING:
        {
            StringResult* strRes =
                static_cast<StringResult*>(static_cast<txAExprResult*>
                                       (mValue));
            PRUnichar ch = '\'';
            if (strRes->mValue.FindChar(ch) != kNotFound) {
                ch = '\"';
            }
            aStr.Append(ch);
            aStr.Append(strRes->mValue);
            aStr.Append(ch);
            return;
        }
        case txAExprResult::RESULT_TREE_FRAGMENT:
        {
            aStr.AppendLiteral(" { RTF literal } ");
            return;
        }
    }
}
#endif
