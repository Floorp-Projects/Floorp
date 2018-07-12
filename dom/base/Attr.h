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
#include "nsIAttribute.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

class nsIDocument;

namespace mozilla {
class EventChainPreVisitor;
namespace dom {

// Attribute helper class used to wrap up an attribute with a dom
// object that implements the DOM Attr interface.
class Attr final : public nsIAttribute
{
  virtual ~Attr() {}

public:
  Attr(nsDOMAttributeMap* aAttrMap,
       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
       const nsAString& aValue);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

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

  nsDOMAttributeMap* GetMap()
  {
    return mAttrMap;
  }

  void SetMap(nsDOMAttributeMap *aMap);

  Element* GetElement() const;

  /**
   * Called when our ownerElement is moved into a new document.
   * Updates the nodeinfo of this node.
   */
  nsresult SetOwnerDocument(nsIDocument* aDocument);

  // nsINode interface
  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual uint32_t GetChildCount() const override;
  virtual nsIContent *GetChildAt_Deprecated(uint32_t aIndex) const override;
  virtual int32_t ComputeIndexOf(const nsINode* aPossibleChild) const override;
  virtual nsresult InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                     bool aNotify) override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;
  virtual already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const override;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Attr,
                                                                   nsIAttribute)

  // WebIDL
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetName(nsAString& aName);
  void GetValue(nsAString& aValue);

  void SetValue(const nsAString& aValue, nsIPrincipal* aTriggeringPrincipal, ErrorResult& aRv);
  void SetValue(const nsAString& aValue, ErrorResult& aRv);

  bool Specified() const;

  // XPCOM GetNamespaceURI() is OK
  // XPCOM GetPrefix() is OK
  // XPCOM GetLocalName() is OK

  Element* GetOwnerElement(ErrorResult& aRv);

protected:
  virtual Element* GetNameSpaceElement() override
  {
    return GetElement();
  }

  static bool sInitialized;

private:
  RefPtr<nsDOMAttributeMap> mAttrMap;
  nsString mValue;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Attr_h */
