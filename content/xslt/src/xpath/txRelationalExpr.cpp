/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 *
 * NaN/Infinity code copied from the JS-library with permission from
 * Netscape Communications Corporation: http://www.mozilla.org/js
 * http://lxr.mozilla.org/seamonkey/source/js/src/jsinterp.c
 *
 */

#include "Expr.h"
#include "txNodeSet.h"
#include "XMLDOMUtils.h"
#include "txIXPathContext.h"

/**
 *  Compares the two ExprResults based on XPath 1.0 Recommendation (section 3.4)
 */
PRBool
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

        txNodeSet* nodeSet = NS_STATIC_CAST(txNodeSet*, aLeft);
        nsRefPtr<StringResult> strResult;
        rv = aContext->recycler()->getStringResult(getter_AddRefs(strResult));
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 i;
        for (i = 0; i < nodeSet->size(); ++i) {
            strResult->mValue.Truncate();
            XMLDOMUtils::getNodeValue(nodeSet->get(i), strResult->mValue);
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

        txNodeSet* nodeSet = NS_STATIC_CAST(txNodeSet*, aRight);
        nsRefPtr<StringResult> strResult;
        rv = aContext->recycler()->getStringResult(getter_AddRefs(strResult));
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 i;
        for (i = 0; i < nodeSet->size(); ++i) {
            strResult->mValue.Truncate();
            XMLDOMUtils::getNodeValue(nodeSet->get(i), strResult->mValue);
            if (compareResults(aContext, aLeft, strResult)) {
                return PR_TRUE;
            }
        }

        return PR_FALSE;
    }

    // Neither is a NodeSet
    if (mOp == EQUAL || mOp == NOT_EQUAL) {
        PRBool result;
        nsAString *lString, *rString;

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
#if defined(XP_WIN) || defined(XP_OS2)
            if (Double::isNaN(lval) || Double::isNaN(rval))
                result = PR_FALSE;
            else
                result = lval == rval;
#else
            result = lval == rval;
#endif
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
                result = rStr.Equals(*lString);
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
#if defined(XP_WIN) || defined(XP_OS2)
    if (Double::isNaN(leftDbl) || Double::isNaN(rightDbl))
        return PR_FALSE;
#endif

    switch (mOp) {
        case LESS_THAN:
            return leftDbl < rightDbl;

        case LESS_OR_EQUAL:
            return leftDbl <= rightDbl;

        case GREATER_THAN :
            return leftDbl > rightDbl;

        case GREATER_OR_EQUAL:
            return leftDbl >= rightDbl;
    }

    NS_NOTREACHED("We should have caught all cases");

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

void
RelationalExpr::toString(nsAString& str)
{
    mLeftExpr->toString(str);

    switch (mOp) {
        case NOT_EQUAL:
            str.Append(NS_LITERAL_STRING("!="));
            break;
        case LESS_THAN:
            str.Append(PRUnichar('<'));
            break;
        case LESS_OR_EQUAL:
            str.Append(NS_LITERAL_STRING("<="));
            break;
        case GREATER_THAN :
            str.Append(PRUnichar('>'));
            break;
        case GREATER_OR_EQUAL:
            str.Append(NS_LITERAL_STRING(">="));
            break;
        default:
            str.Append(PRUnichar('='));
            break;
    }

    mRightExpr->toString(str);
}
