/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtom.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "txExpr.h"
#include "txXPathTreeWalker.h"

txNamedAttributeStep::txNamedAttributeStep(int32_t aNsID, nsAtom* aPrefix,
                                           nsAtom* aLocalName)
    : mNamespace(aNsID), mPrefix(aPrefix), mLocalName(aLocalName) {}

nsresult txNamedAttributeStep::evaluate(txIEvalContext* aContext,
                                        txAExprResult** aResult) {
  *aResult = nullptr;

  RefPtr<txNodeSet> nodes;
  nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  txXPathTreeWalker walker(aContext->getContextNode());
  if (walker.moveToNamedAttribute(mLocalName, mNamespace)) {
    rv = nodes->append(walker.getCurrentPosition());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ADDREF(*aResult = nodes);

  return NS_OK;
}

TX_IMPL_EXPR_STUBS_0(txNamedAttributeStep, NODESET_RESULT)

bool txNamedAttributeStep::isSensitiveTo(ContextSensitivity aContext) {
  return !!(aContext & NODE_CONTEXT);
}

#ifdef TX_TO_STRING
void txNamedAttributeStep::toString(nsAString& aDest) {
  aDest.Append(char16_t('@'));
  if (mPrefix) {
    nsAutoString prefix;
    mPrefix->ToString(prefix);
    aDest.Append(prefix);
    aDest.Append(char16_t(':'));
  }
  nsAutoString localName;
  mLocalName->ToString(localName);
  aDest.Append(localName);
}
#endif
