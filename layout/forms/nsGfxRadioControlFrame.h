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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsGfxRadioControlFrame_h___
#define nsGfxRadioControlFrame_h___

#include "nsRadioControlFrame.h"

// nsGfxRadioControlFrame

#define NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX   0 // for additional style contexts
#define NS_GFX_RADIO_CONTROL_FRAME_LAST_CONTEXT_INDEX   0

class nsGfxRadioControlFrame : public nsRadioControlFrame
{
private:
	typedef nsRadioControlFrame Inherited;

public:
  nsGfxRadioControlFrame();
  ~nsGfxRadioControlFrame();

   //nsIRadioControlFrame methods
  NS_IMETHOD SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext);

  NS_IMETHOD GetAdditionalStyleContext(PRInt32 aIndex, 
                                       nsIStyleContext** aStyleContext) const;
  NS_IMETHOD SetAdditionalStyleContext(PRInt32 aIndex, 
                                       nsIStyleContext* aStyleContext);


  //
  // XXX: The following paint methods are TEMPORARY. It is being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.
  //

  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  virtual void PaintRadioButton(nsIPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect);

  ///XXX: End o the temporary methods
#ifdef DEBUG_rods
  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
#endif

protected:

	virtual PRBool	GetRadioState();
	virtual void 		SetRadioState(nsIPresContext* aPresContext, PRBool aValue);

    //GFX-rendered state variables
  PRBool							mChecked;
  nsIStyleContext* 		mRadioButtonFaceStyle;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};



#endif


