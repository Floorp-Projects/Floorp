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
#ifndef nsIObjectFrame_h___
#define nsIObjectFrame_h___

#include "nsIFrame.h"

class nsIPluginInstance;


// {6D10B07D-E75B-11d4-9885-00C04FA0CF4B}
#define NS_IOBJECTFRAME_IID \
{ 0x6d10b07d, 0xe75b, 0x11d4, { 0x98, 0x85, 0x0, 0xc0, 0x4f, 0xa0, 0xcf, 0x4b } }

class nsIObjectFrame : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IOBJECTFRAME_IID)

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList) = 0;
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow) = 0;
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) = 0;
  NS_IMETHOD DidReflow(nsIPresContext* aPresContext,
                       nsDidReflowStatus aStatus) = 0;
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer) = 0;

  NS_IMETHOD  HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus) = 0;

  NS_IMETHOD Scrolled(nsIView *aView) = 0;
  NS_IMETHOD GetFrameType(nsIAtom** aType) const = 0;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const = 0;
#endif

  NS_IMETHOD Destroy(nsIPresContext* aPresContext) = 0;

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent) = 0;

  NS_IMETHOD GetPluginInstance(nsIPluginInstance*& aPluginInstance) = 0;
};


#endif /* nsIObjectFrame_h___ */
