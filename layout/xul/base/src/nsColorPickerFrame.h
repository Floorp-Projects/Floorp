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

//
// nsColorPickerFrame
//

#ifndef nsColorPickerFrame_h__
#define nsColorPickerFrame_h__


#include "nsLeafFrame.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsIColorPicker.h"

class nsString;


nsresult NS_NewColorPickerFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;


class nsColorPickerFrame : public nsLeafFrame
{
public:
  nsColorPickerFrame();
  virtual ~nsColorPickerFrame();

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("ColorPickerFrame", aResult);
  }
#endif

  // nsIFrame overrides
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  nsresult HandleMouseDownEvent(nsIPresContext* aPresContext,
                                nsGUIEvent*     aEvent,
                                nsEventStatus*  aEventStatus);

  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);
  
protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize) ;


private:
  nsIColorPicker *mColorPicker;

}; // class nsColorPickerFrame

#endif
