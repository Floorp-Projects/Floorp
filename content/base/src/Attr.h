/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsINodeInfo.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttr and nsIDOMNode
class Attr MOZ_FINAL : public nsIAttribute,
                       public nsIDOMAttr
{
public:
  Attr(nsDOMAttributeMap* aAttrMap,
       already_AddRefed<nsINodeInfo> aNodeInfo,
       const nsAString& aValue,
       bool aNsAware);
  virtual ~Attr() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNode interface
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  virtual void GetTextContentInternal(nsAString& aTextContent) MOZ_OVERRIDE;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      ErrorResult& aError) MOZ_OVERRIDE;
  virtual void GetNodeValueInternal(nsAString& aNodeValue) MOZ_OVERRIDE;
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    ErrorResult& aError) MOZ_OVERRIDE;

  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

  // nsIAttribute interface
  void SetMap(nsDOMAttributeMap *aMap) MOZ_OVERRIDE;
  Element *GetElement() const;
  nsresult SetOwnerDocument(nsIDocument* aDocument) MOZ_OVERRIDE;

  // nsINode interface
  virtual bool IsNodeOfType(uint32_t aFlags) const MOZ_OVERRIDE;
  virtual uint32_t GetChildCount() const MOZ_OVERRIDE;
  virtual nsIContent *GetChildAt(uint32_t aIndex) const MOZ_OVERRIDE;
  virtual nsIContent * const * GetChildArray(uint32_t* aChildCount) const MOZ_OVERRIDE;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const MOZ_OVERRIDE;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) MOZ_OVERRIDE;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
  virtual already_AddRefed<nsIURI> GetBaseURI() const MOZ_OVERRIDE;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Attr,
                                                                   nsIAttribute)

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // XPCOM GetName() is OK
  // XPCOM GetValue() is OK

  void SetValue(const nsAString& aValue, ErrorResult& aRv);

  bool Specified() const;

  // XPCOM GetNamespaceURI() is OK
  // XPCOM GetPrefix() is OK
  // XPCOM GetLocalName() is OK

protected:
  virtual Element* GetNameSpaceElement()
  {
    return GetElement();
  }

  static bool sInitialized;

private:
  already_AddRefed<nsIAtom> GetNameAtom(nsIContent* aContent);

  nsString mValue;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Attr_h */
