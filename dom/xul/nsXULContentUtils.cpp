/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A package of routines shared by the XUL content code.

 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/Collator.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "nsXULContentUtils.h"
#include "nsLayoutCID.h"
#include "nsString.h"
#include "nsGkAtoms.h"

using namespace mozilla;

//------------------------------------------------------------------------

const mozilla::intl::Collator* nsXULContentUtils::gCollator;

//------------------------------------------------------------------------
// Constructors n' stuff
//

nsresult nsXULContentUtils::Finish() {
  if (gCollator) {
    delete gCollator;
    gCollator = nullptr;
  }

  return NS_OK;
}

const mozilla::intl::Collator* nsXULContentUtils::GetCollator() {
  if (!gCollator) {
    // Lazily initialize the Collator.
    auto result = mozilla::intl::LocaleService::TryCreateComponent<
        mozilla::intl::Collator>();
    if (result.isErr()) {
      NS_ERROR("couldn't create a mozilla::intl::Collator");
      return nullptr;
    }

    auto collator = result.unwrap();

    // Sort in a case-insensitive way, where "base" letters are considered
    // equal, e.g: a = á, a = A, a ≠ b.
    mozilla::intl::Collator::Options options{};
    options.sensitivity = mozilla::intl::Collator::Sensitivity::Base;
    auto optResult = collator->SetOptions(options);
    if (optResult.isErr()) {
      NS_ERROR("couldn't set options for mozilla::intl::Collator");
      return nullptr;
    }
    gCollator = collator.release();
  }

  return gCollator;
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
