/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINodeList_h___
#define nsINodeList_h___

#include "nsWrapperCache.h"
#include "nsIContent.h"

// IID for the nsINodeList interface
#define NS_INODELIST_IID \
{ 0xadb5e54c, 0x6e96, 0x4102, \
 { 0x8d, 0x40, 0xe0, 0x12, 0x3d, 0xcf, 0x48, 0x7a } }

class nsIContent;
class nsINode;

/**
 * An internal interface for a reasonably fast indexOf.
 */
class nsINodeList : public nsISupports,
                    public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODELIST_IID)

  /**
   * Get the index of the given node in the list.  Will return -1 if the node
   * is not in the list.
   */
  virtual int32_t IndexOf(nsIContent* aContent) = 0;

  /**
   * Get the root node for this nodelist.
   */
  virtual nsINode* GetParentObject() = 0;

  virtual uint32_t Length() = 0;
  virtual nsIContent* Item(uint32_t aIndex) = 0;
  nsIContent* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    nsIContent* item = Item(aIndex);
    aFound = !!item;
    return item;
  }

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINodeList, NS_INODELIST_IID)

#endif /* nsINodeList_h___ */
