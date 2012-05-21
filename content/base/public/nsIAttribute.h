/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIAttribute_h___
#define nsIAttribute_h___

#include "nsINode.h"

class nsDOMAttributeMap;
class nsIContent;

#define NS_IATTRIBUTE_IID  \
{ 0x536167ae, 0x8a9c, 0x4712, \
  { 0x8b, 0x61, 0x3, 0x43, 0xf6, 0xbc, 0x64, 0x75 } }

class nsIAttribute : public nsINode
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IATTRIBUTE_IID)

  virtual void SetMap(nsDOMAttributeMap *aMap) = 0;
  
  nsDOMAttributeMap *GetMap()
  {
    return mAttrMap;
  }

  nsINodeInfo *NodeInfo()
  {
    return mNodeInfo;
  }

  virtual nsIContent* GetContent() const = 0;

  /**
   * Called when our ownerElement is moved into a new document.
   * Updates the nodeinfo of this node.
   */
  virtual nsresult SetOwnerDocument(nsIDocument* aDocument) = 0;

protected:
#ifdef MOZILLA_INTERNAL_API
  nsIAttribute(nsDOMAttributeMap *aAttrMap, already_AddRefed<nsINodeInfo> aNodeInfo,
               bool aNsAware)
    : nsINode(aNodeInfo), mAttrMap(aAttrMap), mNsAware(aNsAware)
  {
  }
#endif //MOZILLA_INTERNAL_API

  nsDOMAttributeMap *mAttrMap; // WEAK
  bool mNsAware;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIAttribute, NS_IATTRIBUTE_IID)

#endif /* nsIAttribute_h___ */
