/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsINodeList_h___
#define nsINodeList_h___

#include "nsIDOMNodeList.h"
#include "nsWrapperCache.h"

class nsINode;
class nsIContent;

// IID for the nsINodeList interface
#define NS_INODELIST_IID \
{ 0xe60b773e, 0x5d20, 0x43f6, \
 { 0xb0, 0x8c, 0xfd, 0x65, 0x26, 0xce, 0xe0, 0x7a } }

/**
 * An internal interface for a reasonably fast indexOf.
 */
class nsINodeList : public nsIDOMNodeList,
                    public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INODELIST_IID)

  /**
   * Get the index of the given node in the list.  Will return -1 if the node
   * is not in the list.
   */
  virtual PRInt32 IndexOf(nsIContent* aContent) = 0;

  /**
   * Get the root node for this nodelist.
   */
  virtual nsINode* GetParentObject() = 0;
};

#define NS_NODELIST_OFFSET_AND_INTERFACE_TABLE_BEGIN(_class)                  \
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN_AMBIGUOUS(_class, nsINodeList)

NS_DEFINE_STATIC_IID_ACCESSOR(nsINodeList, NS_INODELIST_IID)

#endif /* nsINodeList_h___ */
