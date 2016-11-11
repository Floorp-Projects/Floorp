/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "nsIAtom.h"
#include "txNodeSet.h"
#include "nsGkAtoms.h"
#include "txIXPathContext.h"

  //-------------------/
 //- VariableRefExpr -/
//-------------------/

/**
 * Creates a VariableRefExpr with the given variable name
**/
VariableRefExpr::VariableRefExpr(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                 int32_t aNSID)
    : mPrefix(aPrefix), mLocalName(aLocalName), mNamespace(aNSID)
{
    NS_ASSERTION(mLocalName, "VariableRefExpr without local name?");
    if (mPrefix == nsGkAtoms::_empty)
        mPrefix = nullptr;
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
VariableRefExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    nsresult rv = aContext->getVariable(mNamespace, mLocalName, *aResult);
    if (NS_FAILED(rv)) {
      // XXX report error, undefined variable
      return rv;
    }
    return NS_OK;
}

TX_IMPL_EXPR_STUBS_0(VariableRefExpr, ANY_RESULT)

bool
VariableRefExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return !!(aContext & VARIABLES_CONTEXT);
}

#ifdef TX_TO_STRING
void
VariableRefExpr::toString(nsAString& aDest)
{
    aDest.Append(char16_t('$'));
    if (mPrefix) {
        nsAutoString prefix;
        mPrefix->ToString(prefix);
        aDest.Append(prefix);
        aDest.Append(char16_t(':'));
    }
    nsAutoString lname;
    mLocalName->ToString(lname);
    aDest.Append(lname);
}
#endif
