/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#ifndef mozilla_dom_Attr_h
#define mozilla_dom_Attr_h

#include "nsIAttribute.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsINodeInfo.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

namespace mozilla {
namespace dom {

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttr and nsIDOMNode
class Attr : public nsIAttribute,
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
  virtual void GetTextContentInternal(nsAString& aTextContent);
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      mozilla::ErrorResult& aError);
  virtual void GetNodeValueInternal(nsAString& aNodeValue);
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError);

  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  // nsIAttribute interface
  void SetMap(nsDOMAttributeMap *aMap);
  nsIContent *GetContent() const;
  nsresult SetOwnerDocument(nsIDocument* aDocument);

  // nsINode interface
  virtual bool IsNodeOfType(uint32_t aFlags) const;
  virtual uint32_t GetChildCount() const;
  virtual nsIContent *GetChildAt(uint32_t aIndex) const;
  virtual nsIContent * const * GetChildArray(uint32_t* aChildCount) const;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const MOZ_OVERRIDE;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, bool aNotify);
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual already_AddRefed<nsIURI> GetBaseURI() const;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Attr,
                                                         nsIAttribute)

  virtual nsXPCClassInfo* GetClassInfo();

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

  Element* GetOwnerElement(ErrorResult& aRv);

protected:
  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    return GetContentInternal();
  }

  static bool sInitialized;

private:
  already_AddRefed<nsIAtom> GetNameAtom(nsIContent* aContent);
  mozilla::dom::Element* GetContentInternal() const;

  nsString mValue;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Attr_h */
