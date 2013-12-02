/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/dom/DocumentFragment.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashtable.h"
#include "nsDocument.h"

class nsIAtom;
class nsIContent;
class nsIDocument;
class nsINodeInfo;
class nsPIDOMWindow;
class nsXBLPrototypeBinding;
class nsTagNameMapEntry;

namespace mozilla {
namespace dom {

class Element;

class ShadowRoot : public DocumentFragment
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ShadowRoot,
                                           DocumentFragment)
  NS_DECL_ISUPPORTS_INHERITED

  ShadowRoot(nsIContent* aContent, already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~ShadowRoot();

  void AddToIdTable(Element* aElement, nsIAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsIAtom* aId);
  static bool PrefEnabled();

  nsISupports* GetParentObject() const
  {
    return mHost;
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static ShadowRoot* FromNode(nsINode* aNode);

  // WebIDL methods.
  Element* GetElementById(const nsAString& aElementId);
  already_AddRefed<nsContentList>
    GetElementsByTagName(const nsAString& aNamespaceURI);
  already_AddRefed<nsContentList>
    GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName);
  already_AddRefed<nsContentList>
    GetElementsByClassName(const nsAString& aClasses);
  void GetInnerHTML(nsAString& aInnerHTML);
  void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError);
protected:
  nsCOMPtr<nsIContent> mHost;
  nsTHashtable<nsIdentifierMapEntry> mIdentifierMap;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_shadowroot_h__

