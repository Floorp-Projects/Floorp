/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**

 
**/

#ifndef nsButtonFrameRenderer_h___
#define nsButtonFrameRenderer_h___

#include "nsCoord.h"
#include "nsIStyleContext.h"
#include "nsCOMPtr.h"
#include "nsFrame.h"

class nsStyleChangeList;


#define NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX  0
#define NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX  1
#define NS_BUTTON_RENDERER_LAST_CONTEXT_INDEX   NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX

class nsButtonFrameRenderer {
public:

	  nsButtonFrameRenderer();
      virtual ~nsButtonFrameRenderer();


	 virtual void PaintButton(nsIPresContext* aPresContext,
							  nsIRenderingContext& aRenderingContext,
							  const nsRect& aDirtyRect,
							  nsFramePaintLayer aWhichLayer,
							  const nsRect& aRect);

	 virtual void PaintOutlineAndFocusBorders(nsIPresContext* aPresContext,
						  nsIRenderingContext& aRenderingContext,
						  const nsRect& aDirtyRect,
						  nsFramePaintLayer aWhichLayer,
						  const nsRect& aRect);

	 virtual void PaintBorderAndBackground(nsIPresContext* aPresContext,
						  nsIRenderingContext& aRenderingContext,
						  const nsRect& aDirtyRect,
						  nsFramePaintLayer aWhichLayer,
						  const nsRect& aRect);

	virtual void SetNameSpace(PRInt32 aNameSpace);
	virtual void SetFrame(nsFrame* aFrame, nsIPresContext* aPresContext);
 
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
	virtual void ReResolveStyles(nsIPresContext* aPresContext);

  virtual void Redraw(nsIPresContext* aPresContext);

  virtual nsIFrame* GetFrame();
  virtual PRInt32 GetNameSpace();

protected:

private:

	// cached styles for focus and outline.
	nsCOMPtr<nsIStyleContext> mBorderStyle;
	nsCOMPtr<nsIStyleContext> mInnerFocusStyle;
	nsCOMPtr<nsIStyleContext> mOuterFocusStyle;

	PRInt32 mNameSpace;
	nsFrame* mFrame;

  nsRect mOutlineRect;
};


#endif

