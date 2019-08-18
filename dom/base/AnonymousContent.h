/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnonymousContent_h
#define mozilla_dom_AnonymousContent_h

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICSSDeclaration.h"
#include "mozilla/dom/Document.h"

namespace mozilla {
namespace dom {

class Element;
class UnrestrictedDoubleOrAnonymousKeyframeAnimationOptions;

class AnonymousContent final {
 public:
  // Ref counting and cycle collection
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnonymousContent)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AnonymousContent)

  explicit AnonymousContent(already_AddRefed<Element> aContentNode);
  Element& ContentNode() { return *mContentNode; }

  Element* GetElementById(const nsAString& aElementId);
  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

  // WebIDL methods
  void SetTextContentForElement(const nsAString& aElementId,
                                const nsAString& aText, ErrorResult& aRv);

  void GetTextContentForElement(const nsAString& aElementId, DOMString& aText,
                                ErrorResult& aRv);

  void SetAttributeForElement(const nsAString& aElementId,
                              const nsAString& aName, const nsAString& aValue,
                              nsIPrincipal* aSubjectPrincipal,
                              ErrorResult& aRv);

  void GetAttributeForElement(const nsAString& aElementId,
                              const nsAString& aName, DOMString& aValue,
                              ErrorResult& aRv);

  void RemoveAttributeForElement(const nsAString& aElementId,
                                 const nsAString& aName, ErrorResult& aRv);

  already_AddRefed<nsISupports> GetCanvasContext(const nsAString& aElementId,
                                                 const nsAString& aContextId,
                                                 ErrorResult& aRv);

  already_AddRefed<Animation> SetAnimationForElement(
      JSContext* aContext, const nsAString& aElementId,
      JS::Handle<JSObject*> aKeyframes,
      const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
      ErrorResult& aError);

  void SetCutoutRectsForElement(const nsAString& aElementId,
                                const Sequence<OwningNonNull<DOMRect>>& aRects,
                                ErrorResult& aError);

  void GetComputedStylePropertyValue(const nsAString& aElementId,
                                     const nsAString& aPropertyName,
                                     DOMString& aResult, ErrorResult& aRv);

  void GetTargetIdForEvent(Event& aEvent, DOMString& aResult);

  void SetStyle(const nsAString& aProperty, const nsAString& aValue,
                ErrorResult& aRv);

 private:
  ~AnonymousContent();
  RefPtr<Element> mContentNode;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AnonymousContent_h
