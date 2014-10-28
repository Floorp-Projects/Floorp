/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnonymousContent_h
#define mozilla_dom_AnonymousContent_h

#include "mozilla/dom/Element.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICSSDeclaration.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

class Element;

class AnonymousContent MOZ_FINAL
{
public:
  // Ref counting and cycle collection
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnonymousContent)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AnonymousContent)

  explicit AnonymousContent(Element* aContentNode);
  nsCOMPtr<Element> GetContentNode();
  void SetContentNode(Element* aContentNode);
  JSObject* WrapObject(JSContext* aCx);

  // WebIDL methods
  void SetTextContentForElement(const nsAString& aElementId,
                                const nsAString& aText,
                                ErrorResult& aRv);

  void GetTextContentForElement(const nsAString& aElementId,
                                DOMString& aText,
                                ErrorResult& aRv);

  void SetAttributeForElement(const nsAString& aElementId,
                              const nsAString& aName,
                              const nsAString& aValue,
                              ErrorResult& aRv);

  void GetAttributeForElement(const nsAString& aElementId,
                              const nsAString& aName,
                              DOMString& aValue,
                              ErrorResult& aRv);

  void RemoveAttributeForElement(const nsAString& aElementId,
                                 const nsAString& aName,
                                 ErrorResult& aRv);

private:
  ~AnonymousContent();
  Element* GetElementById(const nsAString& aElementId);
  nsCOMPtr<Element> mContentNode;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_AnonymousContent_h
