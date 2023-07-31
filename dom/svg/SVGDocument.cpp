/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDocument.h"

#include "mozilla/css/Loader.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsLiteralString.h"
#include "mozilla/dom/Element.h"
#include "SVGElement.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

using namespace mozilla::css;
using namespace mozilla::dom;

namespace mozilla::dom {

//----------------------------------------------------------------------
// Implementation

nsresult SVGDocument::Clone(dom::NodeInfo* aNodeInfo, nsINode** aResult) const {
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  RefPtr<SVGDocument> clone = new SVGDocument();
  nsresult rv = CloneDocHelper(clone.get());
  NS_ENSURE_SUCCESS(rv, rv);

  clone.forget(aResult);
  return NS_OK;
}

}  // namespace mozilla::dom

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult NS_NewSVGDocument(Document** aInstancePtrResult,
                           nsIPrincipal* aPrincipal,
                           nsIPrincipal* aPartitionedPrincipal) {
  RefPtr<SVGDocument> doc = new SVGDocument();

  nsresult rv = doc->Init(aPrincipal, aPartitionedPrincipal);
  if (NS_FAILED(rv)) {
    return rv;
  }

  doc.forget(aInstancePtrResult);
  return rv;
}
