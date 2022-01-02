/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "nsAtom.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"

#ifdef TX_TO_STRING
#  include "nsReadableUtils.h"
#endif

/**
 * This class represents a FunctionCall as defined by the XSL Working Draft
 **/

//------------------/
//- Public Methods -/
//------------------/

/*
 * Evaluates the given Expression and converts its result to a number.
 */
// static
nsresult FunctionCall::evaluateToNumber(Expr* aExpr, txIEvalContext* aContext,
                                        double* aResult) {
  NS_ASSERTION(aExpr, "missing expression");
  RefPtr<txAExprResult> exprResult;
  nsresult rv = aExpr->evaluate(aContext, getter_AddRefs(exprResult));
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = exprResult->numberValue();

  return NS_OK;
}

/*
 * Evaluates the given Expression and converts its result to a NodeSet.
 * If the result is not a NodeSet nullptr is returned.
 */
nsresult FunctionCall::evaluateToNodeSet(Expr* aExpr, txIEvalContext* aContext,
                                         txNodeSet** aResult) {
  NS_ASSERTION(aExpr, "Missing expression to evaluate");
  *aResult = nullptr;

  RefPtr<txAExprResult> exprRes;
  nsresult rv = aExpr->evaluate(aContext, getter_AddRefs(exprRes));
  NS_ENSURE_SUCCESS(rv, rv);

  if (exprRes->getResultType() != txAExprResult::NODESET) {
    aContext->receiveError(u"NodeSet expected as argument"_ns,
                           NS_ERROR_XSLT_NODESET_EXPECTED);
    return NS_ERROR_XSLT_NODESET_EXPECTED;
  }

  *aResult = static_cast<txNodeSet*>(static_cast<txAExprResult*>(exprRes));
  NS_ADDREF(*aResult);

  return NS_OK;
}

bool FunctionCall::requireParams(int32_t aParamCountMin, int32_t aParamCountMax,
                                 txIEvalContext* aContext) {
  int32_t argc = mParams.Length();
  if (argc < aParamCountMin || (aParamCountMax > -1 && argc > aParamCountMax)) {
    nsAutoString err(u"invalid number of parameters for function"_ns);
#ifdef TX_TO_STRING
    err.AppendLiteral(": ");
    toString(err);
#endif
    aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);

    return false;
  }

  return true;
}

Expr* FunctionCall::getSubExprAt(uint32_t aPos) {
  return mParams.SafeElementAt(aPos);
}

void FunctionCall::setSubExprAt(uint32_t aPos, Expr* aExpr) {
  NS_ASSERTION(aPos < mParams.Length(), "setting bad subexpression index");
  mParams[aPos] = aExpr;
}

bool FunctionCall::argsSensitiveTo(ContextSensitivity aContext) {
  uint32_t i, len = mParams.Length();
  for (i = 0; i < len; ++i) {
    if (mParams[i]->isSensitiveTo(aContext)) {
      return true;
    }
  }

  return false;
}

#ifdef TX_TO_STRING
void FunctionCall::toString(nsAString& aDest) {
  appendName(aDest);
  aDest.AppendLiteral("(");
  StringJoinAppend(
      aDest, u","_ns, mParams,
      [](nsAString& dest, const auto& param) { param->toString(dest); });
  aDest.Append(char16_t(')'));
}
#endif
