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
#ifndef nsIFloaterContainer_h___
#define nsIFloaterContainer_h___

#include <stdio.h>
class PlaceholderFrame;
class nsIPresContext;

// IID for the nsIFloaterContainer interface {5A305EE0-CB55-11d1-8556-00A02468FAB6}
#define NS_IFLOATERCONTAINER_IID         \
{ 0x5a305ee0, 0xcb55, 0x11d1, \
  {0x85, 0x56, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * An interface for communicating with the enclosing block-level container
 * about floating elements. Note that this interface is not an nsISupports
 * interface, and therefore you cannot QueryInterface() back
 */
class nsIFloaterContainer
{
public:
  /**
   * Notification of a newly created floating element.
   *
   * @param aFloater the floating element
   * @param aPlaceholder the placeholder frame associated with the floating
   *          element.
   */
  virtual PRBool AddFloater(nsIPresContext*   aPresContext,
                            nsIFrame*         aFloater,
                            PlaceholderFrame* aPlaceholder) = 0;

 /**
  * Call to place the floating element, because the placeholder frame has been
  * reflowed.
  *
  * XXX This is kind of hokey and unreliable, because it relies on the placeholder
  * getting reflowed. That won't necessarily happen if its geometric parent just
  * decides to move it... It would probably be better if the implementor of this
  * interface managed the tracking of the placeholder within the containing line
  **/
  virtual void PlaceFloater(nsIPresContext*   aPresContext,
                            nsIFrame*         aFloater,
                            PlaceholderFrame* aPlaceholder) = 0;
};

#endif /* nsIFloaterContainer_h___ */
