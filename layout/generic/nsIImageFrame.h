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
#ifndef nsIImageFrame_h___
#define nsIImageFrame_h___

#include "nsIPresContext.h"

class nsIFrame;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;

// {B261A0D5-E696-11d4-9885-00C04FA0CF4B}
#define NS_IIMAGEFRAME_IID \
{ 0xb261a0d5, 0xe696, 0x11d4, { 0x98, 0x85, 0x0, 0xc0, 0x4f, 0xa0, 0xcf, 0x4b } }

class nsIImageFrame : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMAGEFRAME_IID)

  NS_IMETHOD Destroy(nsIPresContext* aPresContext) = 0;
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow) = 0;
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer) = 0;
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) = 0;
  NS_IMETHOD  GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent) = 0;
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) = 0;
  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                       nsPoint& aPoint,
                       PRInt32& aCursor) = 0;
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint) = 0;
  NS_IMETHOD GetFrameType(nsIAtom** aResult) const = 0;
  NS_IMETHOD GetIntrinsicImageSize(nsSize& aSize) = 0;

  NS_IMETHOD GetNaturalImageSize(PRUint32* naturalWidth, 
                                 PRUint32 *naturalHeight) = 0;

  NS_IMETHOD IsImageComplete(PRBool* aComplete) = 0;

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const = 0;
#endif
};

#endif /* nsIImageFrame_h___ */
