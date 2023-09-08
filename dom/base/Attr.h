/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's Attr node.
 */

#ifndef mozilla_dom_Attr_h
#define mozilla_dom_Attr_h

#include "mozilla/Attributes.h"
#include "nsDOMAttributeMap.h"
#include "nsINode.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

namespace mozilla {
class EventChainPreVisitor;
namespace dom {

class Document;

// Attribute helper class used to wrap up an attribute with a dom
// object that implements the DOM Attr interface.
class Attr final : public nsINode {
  virtual ~Attr() = default;

 public:
  Attr(nsDOMAttributeMap* aAttrMap, already_AddRefed<dom::NodeInfo>&& aNodeInfo,
       const nsAString& aValue);

  NS_DECL_ISUPPORTS_INHERITED
  NS_IMETHOD_(void) DeleteCycleCollectable(void) final;

  NS_DECL_DOMARENA_DESTROY

  NS_IMPL_FROMNODE_HELPER(Attr, IsAttr())

  // nsINode interface
  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      OOMReporter& aError) override;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      nsIPrincipal* aSubjectPrincipal,
                                      ErrorResult& aError) override;
  virtual void GetNodeValueInternal(nsAString& aNodeValue) override;
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    ErrorResult& aError) override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  void ConstructUbiNode(void* storage) override;

  nsDOMAttributeMap* GetMap() { return mAttrMap; }

  void SetMap(nsDOMAttributeMap* aMap);

  Element* GetElement() const;

  /**
   * Called when our ownerElement is moved into a new document.
   * Updates the nodeinfo of this node.
   */
  nsresult SetOwnerDocument(Document* aDocument);

  // nsINode interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  nsIURI* GetBaseURI(bool aTryUseXHRDocBaseURI = false) const override;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS(Attr)

  // WebIDL
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  void GetName(nsAString& aName);
  void GetValue(nsAString& aValue);

  void SetValue(const nsAString& aValue, nsIPrincipal* aTriggeringPrincipal,
                ErrorResult& aRv);
  void SetValue(const nsAString& aValue, ErrorResult& aRv);

  bool Specified() const;

  // XPCOM GetNamespaceURI() is OK
  // XPCOM GetPrefix() is OK
  // XPCOM GetLocalName() is OK

  Element* GetOwnerElement();

 protected:
  virtual Element* GetNameSpaceElement() override { return GetElement(); }

  static bool sInitialized;

 private:
  RefPtr<nsDOMAttributeMap> mAttrMap;
  nsString mValue;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_Attr_h */
