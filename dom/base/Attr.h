/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#ifndef mozilla_dom_Attr_h
#define mozilla_dom_Attr_h

#include "mozilla/Attributes.h"
#include "nsIAttribute.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

class nsIDocument;

namespace mozilla {
class EventChainPreVisitor;
namespace dom {

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttr and nsIDOMNode
class Attr final : public nsIAttribute,
                   public nsIDOMAttr
{
  virtual ~Attr() {}

public:
  Attr(nsDOMAttributeMap* aAttrMap,
       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
       const nsAString& aValue);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNode interface
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      ErrorResult& aError) override;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      ErrorResult& aError) override;
  virtual void GetNodeValueInternal(nsAString& aNodeValue) override;
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    ErrorResult& aError) override;

  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR

  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) override;

  // nsIAttribute interface
  void SetMap(nsDOMAttributeMap *aMap) override;
  Element* GetElement() const;
  nsresult SetOwnerDocument(nsIDocument* aDocument) override;

  // nsINode interface
  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual uint32_t GetChildCount() const override;
  virtual nsIContent *GetChildAt(uint32_t aIndex) const override;
  virtual nsIContent * const * GetChildArray(uint32_t* aChildCount) const override;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const override;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) override;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) override;
  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;
  virtual already_AddRefed<nsIURI> GetBaseURI(bool aTryUseXHRDocBaseURI = false) const override;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Attr,
                                                                   nsIAttribute)

  virtual nsIDOMNode* AsDOMNode() override { return this; }

  // WebIDL
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // XPCOM GetName() is OK
  // XPCOM GetValue() is OK

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
  nsString mValue;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Attr_h */
