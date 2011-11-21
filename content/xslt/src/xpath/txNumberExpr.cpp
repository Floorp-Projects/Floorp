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

#include "txExpr.h"
#include <math.h>
#include "txIXPathContext.h"

nsresult
txNumberExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mRightExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    double rightDbl = exprRes->numberValue();

    rv = mLeftExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    double leftDbl = exprRes->numberValue();
    double result = 0;

    switch (mOp) {
        case ADD:
            result = leftDbl + rightDbl;
            break;

        case SUBTRACT:
            result = leftDbl - rightDbl;
            break;

        case DIVIDE:
            if (rightDbl == 0) {
#if defined(XP_WIN)
                /* XXX MSVC miscompiles such that (NaN == 0) */
                if (txDouble::isNaN(rightDbl))
                    result = txDouble::NaN;
                else
#endif
                if (leftDbl == 0 || txDouble::isNaN(leftDbl))
                    result = txDouble::NaN;
                else if (txDouble::isNeg(leftDbl) ^ txDouble::isNeg(rightDbl))
                    result = txDouble::NEGATIVE_INFINITY;
                else
                    result = txDouble::POSITIVE_INFINITY;
            }
            else
                result = leftDbl / rightDbl;
            break;

        case MODULUS:
            if (rightDbl == 0) {
                result = txDouble::NaN;
            }
            else {
#if defined(XP_WIN)
                /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
                if (!txDouble::isInfinite(leftDbl) && txDouble::isInfinite(rightDbl))
                    result = leftDbl;
                else
#endif
                result = fmod(leftDbl, rightDbl);
            }
            break;

        case MULTIPLY:
            result = leftDbl * rightDbl;
            break;
    }

    return aContext->recycler()->getNumberResult(result, aResult);
} //-- evaluate

TX_IMPL_EXPR_STUBS_2(txNumberExpr, NUMBER_RESULT, mLeftExpr, mRightExpr)

bool
txNumberExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return mLeftExpr->isSensitiveTo(aContext) ||
           mRightExpr->isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void
txNumberExpr::toString(nsAString& str)
{
    mLeftExpr->toString(str);

    switch (mOp) {
        case ADD:
            str.AppendLiteral(" + ");
            break;
        case SUBTRACT:
            str.AppendLiteral(" - ");
            break;
        case DIVIDE:
            str.AppendLiteral(" div ");
            break;
        case MODULUS:
            str.AppendLiteral(" mod ");
            break;
        case MULTIPLY:
            str.AppendLiteral(" * ");
            break;
    }

    mRightExpr->toString(str);

}
#endif
