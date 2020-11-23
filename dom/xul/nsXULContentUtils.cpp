/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A package of routines shared by the XUL content code.

 */

#include "mozilla/ArrayUtils.h"

#include "nsCollationCID.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIContent.h"
#include "nsICollation.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "nsXULContentUtils.h"
#include "nsLayoutCID.h"
#include "nsString.h"
#include "nsGkAtoms.h"

using namespace mozilla;

//------------------------------------------------------------------------

nsICollation* nsXULContentUtils::gCollation;

//------------------------------------------------------------------------
// Constructors n' stuff
//

nsresult nsXULContentUtils::Finish() {
  NS_IF_RELEASE(gCollation);

  return NS_OK;
}

nsICollation* nsXULContentUtils::GetCollation() {
  if (!gCollation) {
    nsCOMPtr<nsICollationFactory> colFactory =
        do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
    if (colFactory) {
      DebugOnly<nsresult> rv = colFactory->CreateCollation(&gCollation);
      NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't create collation instance");
    } else
      NS_ERROR("couldn't create instance of collation factory");
  }

  return gCollation;
}

//------------------------------------------------------------------------
//

nsresult nsXULContentUtils::FindChildByTag(nsIContent* aElement,
                                           int32_t aNameSpaceID, nsAtom* aTag,
                                           mozilla::dom::Element** aResult) {
  for (nsIContent* child = aElement->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsElement() && child->NodeInfo()->Equals(aTag, aNameSpaceID)) {
      NS_ADDREF(*aResult = child->AsElement());
      return NS_OK;
    }
  }

  *aResult = nullptr;
  return NS_RDF_NO_VALUE;  // not found
}
