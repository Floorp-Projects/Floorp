/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIAttribute_h___
#define nsIAttribute_h___

#include "nsINode.h"

class nsDOMAttributeMap;

#define NS_IATTRIBUTE_IID  \
{ 0x84d43da7, 0xb45d, 0x47ae, \
  { 0x8f, 0xbf, 0x95, 0x26, 0x78, 0x4d, 0x5e, 0x47 } }

class nsIAttribute : public nsINode
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IATTRIBUTE_IID)

  virtual void SetMap(nsDOMAttributeMap *aMap) = 0;

  nsDOMAttributeMap *GetMap()
  {
    return mAttrMap;
  }

  mozilla::dom::NodeInfo *NodeInfo() const
  {
    return mNodeInfo;
  }

  /**
   * Called when our ownerElement is moved into a new document.
   * Updates the nodeinfo of this node.
   */
  virtual nsresult SetOwnerDocument(nsIDocument* aDocument) = 0;

protected:
#ifdef MOZILLA_INTERNAL_API
  nsIAttribute(nsDOMAttributeMap *aAttrMap,
               already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
#endif //MOZILLA_INTERNAL_API
  virtual ~nsIAttribute();

  RefPtr<nsDOMAttributeMap> mAttrMap;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIAttribute, NS_IATTRIBUTE_IID)

#endif /* nsIAttribute_h___ */
