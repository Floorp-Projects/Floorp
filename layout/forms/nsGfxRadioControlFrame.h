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

#ifndef nsGfxRadioControlFrame_h___
#define nsGfxRadioControlFrame_h___

#include "nsRadioControlFrame.h"

// nsGfxRadioControlFrame

class nsGfxRadioControlFrame : public nsRadioControlFrame
{
private:
	typedef nsRadioControlFrame Inherited;

public:
  nsGfxRadioControlFrame();
  ~nsGfxRadioControlFrame();

   //nsIRadioControlFrame methods
  NS_IMETHOD SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext);

  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aParentContext,
                                   PRInt32 aParentChange,
                                   nsStyleChangeList* aChangeList,
                                   PRInt32* aLocalChange);

  //
  // XXX: The following paint methods are TEMPORARY. It is being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.
  //

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  virtual void PaintRadioButton(nsIPresContext& aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect);

  ///XXX: End o the temporary methods

protected:

	virtual PRBool	GetRadioState();
	virtual void 		SetRadioState(PRBool aValue);

    //GFX-rendered state variables
  PRBool							mChecked;
  nsIStyleContext* 		mRadioButtonFaceStyle;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};



#endif


