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
 * Contributor(s): 
 */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a deck of cards.
 
**/

#ifndef nsSpringFrame_h___
#define nsSpringFrame_h___

#include "nsLeafBoxFrame.h"
struct nsPoint;

class nsSpringFrame : public nsLeafBoxFrame
{
public:

  friend nsresult NS_NewSpringFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

 
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame);

  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      aResult = "Spring";
      return NS_OK;
  }

  nsSpringFrame(nsIPresShell* aShell):nsLeafBoxFrame(aShell) {}
}; // class nsSpringFrame



#endif

