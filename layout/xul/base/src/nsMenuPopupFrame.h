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

//
// nsMenuPopupFrame
//

#ifndef nsMenuPopupFrame_h__
#define nsMenuPopupFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

#include "nsBoxFrame.h"
#include "nsIMenuParent.h"

nsresult NS_NewMenuPopupFrame(nsIFrame** aResult) ;

class nsIViewManager;
class nsIView;

class nsMenuPopupFrame : public nsBoxFrame, public nsIMenuParent
{
public:
  nsMenuPopupFrame();

  NS_DECL_ISUPPORTS

  // nsIMenuParentInterface
  NS_IMETHOD SetCurrentMenuItem(nsIFrame* aMenuItem);
  NS_IMETHOD GetNextMenuItem(nsIFrame* aStart, nsIFrame** aResult);
  NS_IMETHOD GetPreviousMenuItem(nsIFrame* aStart, nsIFrame** aResult);
  NS_IMETHOD SetActive() { return NS_OK; }; // We don't care.

  // Overridden methods
  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow);

  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);

  void GetViewOffset(nsIViewManager* aManager, nsIView* aView, nsPoint& aPoint);
  nsresult SyncViewWithFrame(PRBool aOnMenuBar);

  NS_IMETHOD CaptureMouseEvents(PRBool aGrabMouseEvents);

  void KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag);
  void ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag);

protected:
  nsIFrame* mCurrentMenu; // The current menu that is active.
  PRBool mIsCapturingMouseEvents; // Whether or not we're grabbing the mouse events.
}; // class nsMenuPopupFrame

#endif
