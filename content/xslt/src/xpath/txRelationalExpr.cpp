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
#include "NodeSet.h"
#include "XMLDOMUtils.h"

RelationalExpr::RelationalExpr(Expr* aLeftExpr, Expr* aRightExpr,
                               RelationalExprType aOp)
    : mLeftExpr(aLeftExpr), mRightExpr(aRightExpr), mOp(aOp)
{
}

/**
 *  Compares the two ExprResults based on XPath 1.0 Recommendation (section 3.4)
 */
PRBool
RelationalExpr::compareResults(ExprResult* aLeft, ExprResult* aRight)
{
    short ltype = aLeft->getResultType();
    short rtype = aRight->getResultType();

    // Handle case for just Left NodeSet or Both NodeSets
    if (ltype == ExprResult::NODESET) {
        if (rtype == ExprResult::BOOLEAN) {
            BooleanResult leftBool(aLeft->booleanValue());
            return compareResults(&leftBool, aRight);
        }

        NodeSet* nodeSet = NS_STATIC_CAST(NodeSet*, aLeft);
        StringResult strResult;
        int i;
        for (i = 0; i < nodeSet->size(); ++i) {
            Node* node = nodeSet->get(i);
            strResult.mValue.Truncate();
            XMLDOMUtils::getNodeValue(node, strResult.mValue);
            if (compareResults(&strResult, aRight)) {
                return PR_TRUE;
            }
        }

        return PR_FALSE;
    }

    // Handle case for Just Right NodeSet
    if (rtype == ExprResult::NODESET) {
        if (ltype == ExprResult::BOOLEAN) {
            BooleanResult rightBool(aRight->booleanValue());
            return compareResults(aLeft, &rightBool);
        }

        NodeSet* nodeSet = NS_STATIC_CAST(NodeSet*, aRight);
        StringResult strResult;
        int i;
        for (i = 0; i < nodeSet->size(); ++i) {
            Node* node = nodeSet->get(i);
            strResult.mValue.Truncate();
            XMLDOMUtils::getNodeValue(node, strResult.mValue);
            if (compareResults(aLeft, &strResult)) {
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
        if (ltype == ExprResult::BOOLEAN || rtype == ExprResult::BOOLEAN) {
            result = aLeft->booleanValue() == aRight->booleanValue();
        }

        // If either is a number, compare as numbers.
        else if (ltype == ExprResult::NUMBER || rtype == ExprResult::NUMBER) {
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

    NS_NOTREACHED("We should have cought all cases");

    return PR_FALSE;
}

ExprResult*
RelationalExpr::evaluate(txIEvalContext* aContext)
{
    nsAutoPtr<ExprResult> lResult(mLeftExpr->evaluate(aContext));
    NS_ENSURE_TRUE(lResult, nsnull);

    nsAutoPtr<ExprResult> rResult(mRightExpr->evaluate(aContext));
    NS_ENSURE_TRUE(rResult, nsnull);

    return new BooleanResult(compareResults(lResult, rResult));
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
