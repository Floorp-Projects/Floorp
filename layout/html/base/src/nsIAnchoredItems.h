/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIAnchoredItems_h___
#define nsIAnchoredItems_h___

#include "nsIFrame.h"

// 7327a0a0-bc8d-11d1-8539-00a02468fab6
#define NS_IANCHORED_ITEMS_IID \
 { 0x7327a0a0, 0xbc8d, 0x11d1, {0x85, 0x39, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * An interface for managing anchored items. Note that this interface is not
 * an nsISupports interface, and therefore you cannot QueryInterface() back
 */
class nsIAnchoredItems
{
public:
  enum AnchoringPosition {anHTMLFloater};

  /**
   * Request to add an anchored item to this geometric parent. This
   * is for handling anchored items that are not displayed inline (at
   * the current point).
   */
  virtual void  AddAnchoredItem(nsIFrame*         aAnchoredItem,
                                AnchoringPosition aPosition,
                                nsIFrame*         aContainer) = 0;

 /**
  * Called to remove an anchored item, most likely because the associated
  * piece of content has been removed.
  */
  virtual void RemoveAnchoredItem(nsIFrame* aAnchoredItem) = 0;
};

#endif /* nsIAnchoredItems_h___ */
