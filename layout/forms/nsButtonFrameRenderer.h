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

class nsStyleChangeList;

#define NS_BUTTON_RENDERER_OUTLINE_CONTEXT_INDEX      0
#define NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX  1
#define NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX  2
#define NS_BUTTON_RENDERER_LAST_CONTEXT_INDEX   NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX

class nsButtonFrameRenderer {
public:

	  nsButtonFrameRenderer();
      virtual ~nsButtonFrameRenderer();


	 virtual void PaintButton(nsIPresContext& aPresContext,
							  nsIRenderingContext& aRenderingContext,
							  const nsRect& aDirtyRect,
							  nsFramePaintLayer aWhichLayer,
							  const nsRect& aRect);

	 virtual void PaintOutlineAndFocusBorders(nsIPresContext& aPresContext,
						  nsIRenderingContext& aRenderingContext,
						  const nsRect& aDirtyRect,
						  nsFramePaintLayer aWhichLayer,
						  const nsRect& aRect);

	 virtual void PaintBorderAndBackground(nsIPresContext& aPresContext,
						  nsIRenderingContext& aRenderingContext,
						  const nsRect& aDirtyRect,
						  nsFramePaintLayer aWhichLayer,
						  const nsRect& aRect);

	virtual void SetNameSpace(PRInt32 aNameSpace);
	virtual void SetFrame(nsFrame* aFrame, nsIPresContext& aPresContext);
 
	virtual void SetDisabled(PRBool aDisabled, PRBool notify);

	PRBool isActive();
	PRBool isDisabled();

	virtual void GetButtonOutlineRect(const nsRect& aRect, nsRect& aResult);
	virtual void GetButtonOuterFocusRect(const nsRect& aRect, nsRect& aResult);
	virtual void GetButtonRect(const nsRect& aRect, nsRect& aResult);
	virtual void GetButtonInnerFocusRect(const nsRect& aRect, nsRect& aResult);
	virtual void GetButtonContentRect(const nsRect& aRect, nsRect& aResult);
  virtual nsMargin GetButtonOuterFocusBorderAndPadding();
  virtual nsMargin GetButtonBorderAndPadding();
  virtual nsMargin GetButtonInnerFocusMargin();
  virtual nsMargin GetButtonInnerFocusBorderAndPadding();
  virtual nsMargin GetButtonOutlineBorderAndPadding();
  virtual nsMargin GetFullButtonBorderAndPadding();
  virtual nsMargin GetAddedButtonBorderAndPadding();

  virtual nsresult GetStyleContext(PRInt32 aIndex, nsIStyleContext** aStyleContext) const;
  virtual nsresult SetStyleContext(PRInt32 aIndex, nsIStyleContext* aStyleContext);
	virtual void ReResolveStyles(nsIPresContext& aPresContext,
                               PRInt32 aParentChange,
                               nsStyleChangeList* aChangeList,
                               PRInt32* aLocalChange);

  virtual void Redraw();

protected:

private:

	// cached styles for focus and outline.
	nsCOMPtr<nsIStyleContext> mBorderStyle;
	nsCOMPtr<nsIStyleContext> mInnerFocusStyle;
	nsCOMPtr<nsIStyleContext> mOuterFocusStyle;
	nsCOMPtr<nsIStyleContext> mOutlineStyle;

	PRInt32 mNameSpace;
	nsFrame* mFrame;

  nsRect mOutlineRect;
};


#endif

