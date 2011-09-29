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
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
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

#include "nsContentUtils.h"
#include "txExpr.h"
#include "txNodeSet.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

/**
 *  Compares the two ExprResults based on XPath 1.0 Recommendation (section 3.4)
 */
bool
RelationalExpr::compareResults(txIEvalContext* aContext, txAExprResult* aLeft,
                               txAExprResult* aRight)
{
    short ltype = aLeft->getResultType();
    short rtype = aRight->getResultType();
    nsresult rv = NS_OK;

    // Handle case for just Left NodeSet or Both NodeSets
    if (ltype == txAExprResult::NODESET) {
        if (rtype == txAExprResult::BOOLEAN) {
            BooleanResult leftBool(aLeft->booleanValue());
            return compareResults(aContext, &leftBool, aRight);
        }

        txNodeSet* nodeSet = static_cast<txNodeSet*>(aLeft);
        nsRefPtr<StringResult> strResult;
        rv = aContext->recycler()->getStringResult(getter_AddRefs(strResult));
        NS_ENSURE_SUCCESS(rv, PR_FALSE);

        PRInt32 i;
        for (i = 0; i < nodeSet->size(); ++i) {
            strResult->mValue.Truncate();
            txXPathNodeUtils::appendNodeValue(nodeSet->get(i),
                                              strResult->mValue);
            if (compareResults(aContext, strResult, aRight)) {
                return PR_TRUE;
            }
        }

        return PR_FALSE;
    }

    // Handle case for Just Right NodeSet
    if (rtype == txAExprResult::NODESET) {
        if (ltype == txAExprResult::BOOLEAN) {
            BooleanResult rightBool(aRight->booleanValue());
            return compareResults(aContext, aLeft, &rightBool);
        }

        txNodeSet* nodeSet = static_cast<txNodeSet*>(aRight);
        nsRefPtr<StringResult> strResult;
        rv = aContext->recycler()->getStringResult(getter_AddRefs(strResult));
        NS_ENSURE_SUCCESS(rv, PR_FALSE);

        PRInt32 i;
        for (i = 0; i < nodeSet->size(); ++i) {
            strResult->mValue.Truncate();
            txXPathNodeUtils::appendNodeValue(nodeSet->get(i),
                                              strResult->mValue);
            if (compareResults(aContext, aLeft, strResult)) {
                return PR_TRUE;
            }
        }

        return PR_FALSE;
    }

    // Neither is a NodeSet
    if (mOp == EQUAL || mOp == NOT_EQUAL) {
        bool result;
        const nsString *lString, *rString;

        // If either is a bool, compare as bools.
        if (ltype == txAExprResult::BOOLEAN ||
            rtype == txAExprResult::BOOLEAN) {
            result = aLeft->booleanValue() == aRight->booleanValue();
        }

        // If either is a number, compare as numbers.
        else if (ltype == txAExprResult::NUMBER ||
                 rtype == txAExprResult::NUMBER) {
            double lval = aLeft->numberValue();
            double rval = aRight->numberValue();
            result = DOUBLE_COMPARE(lval, ==, rval);
        }

        // Otherwise compare as strings. Try to use the stringobject in
        // StringResult if possible since that is a common case.
        else if ((lString = aLeft->stringValuePointer())) {
            if ((rString = aRight->stringValuePointer())) {
                result = lString->Equals(*rString);
            }
            else {
                nsAutoString rStr;
                aRight->stringValue(rStr);
                result = lString->Equals(rStr);
            }
        }
        else if ((rString = aRight->stringValuePointer())) {
            nsAutoString lStr;
            aLeft->stringValue(lStr);
            result = rString->Equals(lStr);
        }
        else {
            nsAutoString lStr, rStr;
            aLeft->stringValue(lStr);
            aRight->stringValue(rStr);
            result = lStr.Equals(rStr);
        }

        return mOp == EQUAL ? result : !result;
    }

    double leftDbl = aLeft->numberValue();
    double rightDbl = aRight->numberValue();
    switch (mOp) {
        case LESS_THAN:
        {
            return DOUBLE_COMPARE(leftDbl, <, rightDbl);
        }
        case LESS_OR_EQUAL:
        {
            return DOUBLE_COMPARE(leftDbl, <=, rightDbl);
        }
        case GREATER_THAN:
        {
            return DOUBLE_COMPARE(leftDbl, >, rightDbl);
        }
        case GREATER_OR_EQUAL:
        {
            return DOUBLE_COMPARE(leftDbl, >=, rightDbl);
        }
        default:
        {
            NS_NOTREACHED("We should have caught all cases");
        }
    }

    return PR_FALSE;
}

nsresult
RelationalExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;
    nsRefPtr<txAExprResult> lResult;
    nsresult rv = mLeftExpr->evaluate(aContext, getter_AddRefs(lResult));
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<txAExprResult> rResult;
    rv = mRightExpr->evaluate(aContext, getter_AddRefs(rResult));
    NS_ENSURE_SUCCESS(rv, rv);
    
    aContext->recycler()->
        getBoolResult(compareResults(aContext, lResult, rResult), aResult);

    return NS_OK;
}

TX_IMPL_EXPR_STUBS_2(RelationalExpr, BOOLEAN_RESULT, mLeftExpr, mRightExpr)

bool
RelationalExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return mLeftExpr->isSensitiveTo(aContext) ||
           mRightExpr->isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void
RelationalExpr::toString(nsAString& str)
{
    mLeftExpr->toString(str);

    switch (mOp) {
        case NOT_EQUAL:
            str.AppendLiteral("!=");
            break;
        case LESS_THAN:
            str.Append(PRUnichar('<'));
            break;
        case LESS_OR_EQUAL:
            str.AppendLiteral("<=");
            break;
        case GREATER_THAN :
            str.Append(PRUnichar('>'));
            break;
        case GREATER_OR_EQUAL:
            str.AppendLiteral(">=");
            break;
        default:
            str.Append(PRUnichar('='));
            break;
    }

    mRightExpr->toString(str);
}
#endif
