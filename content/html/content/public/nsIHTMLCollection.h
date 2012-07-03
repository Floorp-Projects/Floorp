/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIHTMLCollection_h___
#define nsIHTMLCollection_h___

#include "nsIDOMHTMLCollection.h"

class nsINode;
class nsWrapperCache;

// IID for the nsIHTMLCollection interface
#define NS_IHTMLCOLLECTION_IID \
{ 0xdea91ad6, 0x57d1, 0x4e7a, \
 { 0xb5, 0x5a, 0xdb, 0xfc, 0x36, 0x7b, 0xc8, 0x22 } }

/**
 * An internal interface
 */
class nsIHTMLCollection : public nsIDOMHTMLCollection
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHTMLCOLLECTION_IID)

  /**
   * Get the root node for this HTML collection.
   */
  virtual nsINode* GetParentObject() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLCollection, NS_IHTMLCOLLECTION_IID)

#endif /* nsIHTMLCollection_h___ */
