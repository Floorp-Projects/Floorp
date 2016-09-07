/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "HTMLCanvasElement.h"

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

Element*
AnonymousContent::GetContentNode()
{
  return mContentNode;
}

void
AnonymousContent::SetContentNode(Element* aContentNode)
{
  mContentNode = aContentNode;
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

already_AddRefed<nsISupports>
AnonymousContent::GetCanvasContext(const nsAString& aElementId,
                                   const nsAString& aContextId,
                                   ErrorResult& aRv)
{
  Element* element = GetElementById(aElementId);

  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  if (!element->IsHTMLElement(nsGkAtoms::canvas)) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> context;

  HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(element);
  canvas->GetContext(aContextId, getter_AddRefs(context));

  return context.forget();
}

Element*
AnonymousContent::GetElementById(const nsAString& aElementId)
{
  // This can be made faster in the future if needed.
  nsCOMPtr<nsIAtom> elementId = NS_Atomize(aElementId);
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

bool
AnonymousContent::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto,
                             JS::MutableHandle<JSObject*> aReflector)
{
  return AnonymousContentBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

} // namespace dom
} // namespace mozilla
