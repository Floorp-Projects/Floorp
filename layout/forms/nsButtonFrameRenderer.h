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

/**

 
**/

#ifndef nsButtonFrameRenderer_h___
#define nsButtonFrameRenderer_h___

#include "nsCoord.h"
#include "nsIStyleContext.h"
#include "nsCOMPtr.h"
#include "nsFrame.h"

class nsButtonFrameRenderer {
public:

	  nsButtonFrameRenderer();

	  NS_IMETHOD HandleEvent(nsFrame* aFrame,
		                 nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);


	 virtual void PaintButton(nsFrame* aFrame,nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer,
							const nsString& aLabel,
							const nsRect& aRect);

	 virtual void PaintButtonBorder(nsFrame* aFrame,
										nsIPresContext& aPresContext,
										nsIRenderingContext& aRenderingContext,
										const nsRect& aDirtyRect,
										nsFramePaintLayer aWhichLayer,
										const nsRect& aRect);

	 virtual void PaintButtonContents(nsFrame* aFrame,nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer,
							const nsString& aLabel,
							const nsRect& aRect);

	virtual nscoord GetVerticalInsidePadding(const nsFrame* aFrame, float aPixToTwip,
                                           nscoord aInnerHeight) const;

    virtual nscoord GetHorizontalInsidePadding(const nsFrame* aFrame, 
		                                       nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  

	static void MarginUnion(const nsMargin& a, const nsMargin& b, nsMargin& ab);

	virtual void ResetButtonStyles();

    virtual void UpdateButtonStyles(nsFrame* aFrame, nsIPresContext& aPresContext);

	virtual void GetButtonMargin(nsFrame* aFrame, nsMargin& aMargin);

	virtual void SetDisabled(PRBool disabled);

	virtual void SetFocus(PRBool focus);

	virtual void SetNameSpace(PRInt32 aNameSpaceID);


  PRBool mGotFocus;
  PRBool mPressed;
  PRBool mMouseInside;
  PRBool mDisabledChanged;

  // cached styles for focus and outline.
  nsCOMPtr<nsIStyleContext> mFocusStyle;
  nsCOMPtr<nsIStyleContext> mOutlineStyle;

  // KLUDGE variable to make sure disabling works.
  PRBool mDisabled;
  PRInt32 mNameSpace;

private:

  void CheckDisabled(nsIContent* content);
};

#endif

