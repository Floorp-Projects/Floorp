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

	enum ButtonState {
		active,
		hover,
		normal
	};
	  nsButtonFrameRenderer();
      virtual ~nsButtonFrameRenderer();

	  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
						 nsGUIEvent* aEvent,
						 nsEventStatus& aEventStatus);


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
 
	virtual void SetActive(PRBool aActive, PRBool notify);
	virtual void SetHover(PRBool aHover, PRBool notify);
	virtual void SetFocus(PRBool aFocus, PRBool notify);
	virtual void SetDisabled(PRBool aDisabled, PRBool notify);

	PRBool isActive();
	PRBool isHover();
	PRBool isDisabled();
	PRBool isFocus();

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

	virtual void ReResolveStyles(nsIPresContext& aPresContext);

	/**
	* Subroutine to add in borders and padding
	*/
	virtual void AddFocusBordersAndPadding(nsIPresContext& aPresContext,
							const nsHTMLReflowState& aReflowState,
							nsHTMLReflowMetrics& aDesiredSize,
							nsMargin& aBorderPadding);

  virtual void Redraw();

protected:

    virtual nsString GetPseudoClassAttribute();
    virtual void SetPseudoClassAttribute(const nsString& value, PRBool notify);
    virtual void ToggleClass(PRBool aEnabled, const nsString& c, PRBool notify);
    virtual void AddClass(nsString& pseudoclass, const nsString newClass);
    virtual void RemoveClass(nsString& pseudoclass, const nsString newClass);
    virtual PRInt32 IndexOfClass(nsString& pseudoclass, const nsString& c);

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

