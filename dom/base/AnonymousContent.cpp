/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnonymousContent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/AnonymousContentBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsStyledElement.h"

namespace mozilla {
namespace dom {

// Ref counting and cycle collection
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnonymousContent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnonymousContent, Release)
NS_IMPL_CYCLE_COLLECTION(AnonymousContent, mContentNode)

AnonymousContent::AnonymousContent(Element* aContentNode) :
  mContentNode(aContentNode)
{}

AnonymousContent::~AnonymousContent()
{
}

nsCOMPtr<Element>
AnonymousContent::GetContentNode()
{
  return mContentNode;
}

void
AnonymousContent::SetTextContentForElement(const nsAString& aElementId,
                                           const nsAString& aText,
                                           ErrorResult& aRv)
{
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->SetTextContent(aText, aRv);
}

void
AnonymousContent::GetTextContentForElement(const nsAString& aElementId,
                                           DOMString& aText,
                                           ErrorResult& aRv)
{
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->GetTextContent(aText, aRv);
}

void
AnonymousContent::SetAttributeForElement(const nsAString& aElementId,
                                         const nsAString& aName,
                                         const nsAString& aValue,
                                         ErrorResult& aRv)
{
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->SetAttribute(aName, aValue, aRv);
}

void
AnonymousContent::GetAttributeForElement(const nsAString& aElementId,
                                         const nsAString& aName,
                                         DOMString& aValue,
                                         ErrorResult& aRv)
{
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->GetAttribute(aName, aValue);
}

void
AnonymousContent::RemoveAttributeForElement(const nsAString& aElementId,
                                            const nsAString& aName,
                                            ErrorResult& aRv)
{
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->RemoveAttribute(aName, aRv);
}

Element*
AnonymousContent::GetElementById(const nsAString& aElementId)
{
  // This can be made faster in the future if needed.
  nsCOMPtr<nsIAtom> elementId = do_GetAtom(aElementId);
  for (nsIContent* kid = mContentNode->GetFirstChild(); kid;
       kid = kid->GetNextNode(mContentNode)) {
    if (!kid->IsElement()) {
      continue;
    }
    nsIAtom* id = kid->AsElement()->GetID();
    if (id && id == elementId) {
      return kid->AsElement();
    }
  }
  return nullptr;
}

JSObject*
AnonymousContent::WrapObject(JSContext* aCx)
{
  return AnonymousContentBinding::Wrap(aCx, this);
}

} // dom namespace
} // mozilla namespace
