/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIAbsoluteItems_h___
#define nsIAbsoluteItems_h___

class nsAbsoluteFrame;

// IID for the nsIAbsoluteItems interface {28E68A10-0BD4-11d2-85DD-00A02468FAB6}
#define NS_IABSOLUTE_ITEMS_IID         \
{ 0x28e68a10, 0xbd4, 0x11d2, \
  {0x85, 0xdd, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * An interface for managing absolutely positioned items. Note that this interface
 * is not an nsISupports interface, and therefore you cannot QueryInterface() back
 */
class nsIAbsoluteItems
{
public:
  /**
   * Request to add an absolutely positioned item. The anchor for the
   * absolutely positioned item is passed as an argument
   *
   * @see nsAbsoluteFrame::GetAbsoluteFrame()
   */
  NS_IMETHOD  AddAbsoluteItem(nsAbsoluteFrame* aAnchorFrame) = 0;

 /**
  * Called to remove an absolutely positioned item, most likely because the
  * associated piece of content has been removed.
  **/
  NS_IMETHOD  RemoveAbsoluteItem(nsAbsoluteFrame* aAnchorFrame) = 0;
};

#endif /* nsIAbsoluteItems_h___ */
