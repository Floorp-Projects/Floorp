/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnonymousContent.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/AnonymousContentBinding.h"
#include "nsComputedDOMStyle.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIFrame.h"
#include "nsStyledElement.h"
#include "HTMLCanvasElement.h"

namespace mozilla {
namespace dom {

// Ref counting and cycle collection
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnonymousContent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnonymousContent, Release)
NS_IMPL_CYCLE_COLLECTION(AnonymousContent, mContentNode)

AnonymousContent::AnonymousContent(already_AddRefed<Element> aContentNode)
    : mContentNode(aContentNode) {
  MOZ_ASSERT(mContentNode);
}

AnonymousContent::~AnonymousContent() = default;

void AnonymousContent::SetTextContentForElement(const nsAString& aElementId,
                                                const nsAString& aText,
                                                ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->SetTextContent(aText, aRv);
}

void AnonymousContent::GetTextContentForElement(const nsAString& aElementId,
                                                DOMString& aText,
                                                ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->GetTextContent(aText, aRv);
}

void AnonymousContent::SetAttributeForElement(const nsAString& aElementId,
                                              const nsAString& aName,
                                              const nsAString& aValue,
                                              nsIPrincipal* aSubjectPrincipal,
                                              ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->SetAttribute(aName, aValue, aSubjectPrincipal, aRv);
}

void AnonymousContent::GetAttributeForElement(const nsAString& aElementId,
                                              const nsAString& aName,
                                              DOMString& aValue,
                                              ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->GetAttribute(aName, aValue);
}

void AnonymousContent::RemoveAttributeForElement(const nsAString& aElementId,
                                                 const nsAString& aName,
                                                 ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  element->RemoveAttribute(aName, aRv);
}

already_AddRefed<nsISupports> AnonymousContent::GetCanvasContext(
    const nsAString& aElementId, const nsAString& aContextId,
    ErrorResult& aRv) {
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

already_AddRefed<Animation> AnonymousContent::SetAnimationForElement(
    JSContext* aContext, const nsAString& aElementId,
    JS::Handle<JSObject*> aKeyframes,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);

  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  return element->Animate(aContext, aKeyframes, aOptions, aRv);
}

void AnonymousContent::SetCutoutRectsForElement(
    const nsAString& aElementId, const Sequence<OwningNonNull<DOMRect>>& aRects,
    ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);

  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsRegion cutOutRegion;
  for (const auto& r : aRects) {
    CSSRect rect(r->X(), r->Y(), r->Width(), r->Height());
    cutOutRegion.OrWith(CSSRect::ToAppUnits(rect));
  }

  element->SetProperty(nsGkAtoms::cutoutregion, new nsRegion(cutOutRegion),
                       nsINode::DeleteProperty<nsRegion>);

  nsIFrame* frame = element->GetPrimaryFrame();
  if (frame) {
    frame->SchedulePaint();
  }
}

Element* AnonymousContent::GetElementById(const nsAString& aElementId) {
  // This can be made faster in the future if needed.
  RefPtr<nsAtom> elementId = NS_Atomize(aElementId);
  for (nsIContent* node = mContentNode; node;
       node = node->GetNextNode(mContentNode)) {
    if (!node->IsElement()) {
      continue;
    }
    nsAtom* id = node->AsElement()->GetID();
    if (id && id == elementId) {
      return node->AsElement();
    }
  }
  return nullptr;
}

bool AnonymousContent::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto,
                                  JS::MutableHandle<JSObject*> aReflector) {
  return AnonymousContent_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

void AnonymousContent::GetComputedStylePropertyValue(
    const nsAString& aElementId, const nsAString& aPropertyName,
    DOMString& aResult, ErrorResult& aRv) {
  Element* element = GetElementById(aElementId);
  if (!element) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  if (!element->OwnerDoc()->GetPresShell()) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  RefPtr<nsComputedDOMStyle> cs =
      new nsComputedDOMStyle(element, NS_LITERAL_STRING(""),
                             element->OwnerDoc(), nsComputedDOMStyle::eAll);
  aRv = cs->GetPropertyValue(aPropertyName, aResult);
}

void AnonymousContent::GetTargetIdForEvent(Event& aEvent, DOMString& aResult) {
  nsCOMPtr<Element> el = do_QueryInterface(aEvent.GetOriginalTarget());
  if (el && el->IsInNativeAnonymousSubtree() && mContentNode->Contains(el)) {
    aResult.SetKnownLiveAtom(el->GetID(), DOMString::eTreatNullAsNull);
    return;
  }

  aResult.SetNull();
}

void AnonymousContent::SetStyle(const nsAString& aProperty,
                                const nsAString& aValue, ErrorResult& aRv) {
  if (!mContentNode->IsHTMLElement()) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsGenericHTMLElement* element = nsGenericHTMLElement::FromNode(mContentNode);
  nsCOMPtr<nsICSSDeclaration> declaration = element->Style();
  declaration->SetProperty(aProperty, aValue, EmptyString());
}

}  // namespace dom
}  // namespace mozilla
