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

#ifndef nsInputFrame_h___
#define nsInputFrame_h___

//#include "nsHTMLParts.h"
#include "nsHTMLTagContent.h"
#include "nsISupports.h"
#include "nsIWidget.h"
class nsIView;
//#include "nsIRenderingContext.h"
//#include "nsIPresContext.h"
//#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
//#include "nsCSSRendering.h"
//#include "nsHTMLIIDs.h"

enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseDown,
  eMouseUp
};

class nsInputFrame : public nsLeafFrame {
public:
  nsInputFrame(nsIContent* aContent,
               PRInt32 aIndexInParent,
               nsIFrame* aParentFrame);

  virtual void Paint(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

  virtual ReflowStatus  ResizeReflow(nsIPresContext*  aCX,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsSize*          aMaxElementSize);

  virtual const nsIID GetCID(); // make this pure virtual

  virtual const nsIID GetIID(); // make this pure virtual

  virtual nsresult GetWidget(nsIView* aView, nsIWidget** aWidget);

  virtual void InitializeWidget(nsIView *aView) = 0;  // initialize widget in ResizeReflow

  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                               nsSize& aBounds); // make this pure virtual

  virtual nsEventStatus HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent* aEvent);

  virtual void MouseClicked() {}

protected:
  virtual ~nsInputFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize& aMaxSize);
  PRBool BoundsAreSet();

  nsSize   mCacheBounds;
  nsMouseState mLastMouseState;
};

#endif
