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
#ifndef nsGfxCheckboxControlFrame_h___
#define nsGfxCheckboxControlFrame_h___

#include "nsCheckboxControlFrame.h"


class nsGfxCheckboxControlFrame : public nsCheckboxControlFrame {
private:
	typedef nsCheckboxControlFrame Inherited;

public:
  nsGfxCheckboxControlFrame();
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("CheckboxControl", aResult);
  }
#endif

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
#ifdef DEBUG_rods
  NS_IMETHOD Reflow(nsIPresContext&          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
#endif

protected:
  virtual CheckState GetCheckboxState();
  virtual void SetCheckboxState(nsIPresContext* aPresContext, CheckState aValue);

  virtual void PaintCheckBox(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer);
  virtual void PaintMixedMark(nsIRenderingContext& aRenderingContext,
                               float aPixelsToTwips, const nsRect& aRect) ;

    //GFX-rendered state variables
  CheckState mChecked;
  
};

#endif
