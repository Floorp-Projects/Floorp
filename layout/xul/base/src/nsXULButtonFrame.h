/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#ifndef nsXULButtonFrame_h___
#define nsXULButtonFrame_h___

#include "nsBoxFrame.h"

class nsXULButtonFrame : public nsBoxFrame
{
public:
  friend nsresult NS_NewXULButtonFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  nsXULButtonFrame(nsIPresShell* aPresShell);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsIFrame**     aFrame);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus);

  virtual void MouseClicked (nsIPresContext* aPresContext);


}; // class nsXULButtonFrame

#endif /* nsXULButtonFrame_h___ */
