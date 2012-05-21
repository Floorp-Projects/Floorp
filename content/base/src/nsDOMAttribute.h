/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#ifndef nsDOMAttribute_h___
#define nsDOMAttribute_h___

#include "nsIAttribute.h"
#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsINodeInfo.h"
#include "nsDOMAttributeMap.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttr and nsIDOMNode
class nsDOMAttribute : public nsIAttribute,
                       public nsIDOMAttr
{
public:
  nsDOMAttribute(nsDOMAttributeMap* aAttrMap,
                 already_AddRefed<nsINodeInfo> aNodeInfo,
                 const nsAString& aValue,
                 bool aNsAware);
  virtual ~nsDOMAttribute() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIDOMNode interface
  NS_DECL_NSIDOMNODE

  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  // nsIAttribute interface
  void SetMap(nsDOMAttributeMap *aMap);
  nsIContent *GetContent() const;
  nsresult SetOwnerDocument(nsIDocument* aDocument);

  // nsINode interface
  virtual bool IsNodeOfType(PRUint32 aFlags) const;
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual nsIContent * const * GetChildArray(PRUint32* aChildCount) const;
  virtual PRInt32 IndexOf(nsINode* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 bool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, bool aNotify);
  virtual void RemoveChildAt(PRUint32 aIndex, bool aNotify);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual already_AddRefed<nsIURI> GetBaseURI() const;

  static void Initialize();
  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMAttribute,
                                                         nsIAttribute)

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  virtual mozilla::dom::Element* GetNameSpaceElement()
  {
    return GetContentInternal();
  }

  static bool sInitialized;

private:
  already_AddRefed<nsIAtom> GetNameAtom(nsIContent* aContent);
  mozilla::dom::Element *GetContentInternal() const
  {
    return mAttrMap ? mAttrMap->GetContent() : nsnull;
  }

  nsString mValue;
};


#endif /* nsDOMAttribute_h___ */
