/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DirectionalityUtils.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMNodeFilter.h"
#include "nsTreeWalker.h"
#include "nsIDOMHTMLDocument.h"


namespace mozilla {

namespace directionality {

typedef mozilla::dom::Element Element;

Directionality
RecomputeDirectionality(Element* aElement, bool aNotify)
{
  Directionality dir = eDir_LTR;

  if (aElement->HasValidDir()) {
    dir = aElement->GetDirectionality();
  } else {
    Element* parent = aElement->GetElementParent();
    if (parent) {
      // If the element doesn't have an explicit dir attribute with a valid
      // value, the directionality is the same as the parent element (but
      // don't propagate the parent directionality if it isn't set yet).
      Directionality parentDir = parent->GetDirectionality();
      if (parentDir != eDir_NotSet) {
        dir = parentDir;
      }
    } else {
      // If there is no parent element, the directionality is the same as the
      // document direction.
      Directionality documentDir =
        aElement->OwnerDoc()->GetDocumentDirectionality();
      if (documentDir != eDir_NotSet) {
        dir = documentDir;
      }
    }
    
    aElement->SetDirectionality(dir, aNotify);
  }
  return dir;
}

void
SetDirectionalityOnDescendants(Element* aElement, Directionality aDir,
                               bool aNotify)
{
  for (nsIContent* child = aElement->GetFirstChild(); child; ) {
    if (!child->IsElement()) {
      child = child->GetNextNode(aElement);
      continue;
    }

    Element* element = child->AsElement();
    if (element->HasValidDir()) {
      child = child->GetNextNonChildNode(aElement);
      continue;
    }
    element->SetDirectionality(aDir, aNotify);
    child = child->GetNextNode(aElement);
  }
}

} // end namespace directionality

} // end namespace mozilla

