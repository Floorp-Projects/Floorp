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
#ifndef nsBodyFrame_h___
#define nsBodyFrame_h___

#include "nsBlockFrame.h"
#include "nsIAbsoluteItems.h"
#include "nsISpaceManager.h"
#include "nsVoidArray.h"

class nsSpaceManager;

struct nsStyleDisplay;
struct nsStylePosition;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BODY_FRAME_ABSOLUTE_LIST_INDEX   (NS_BLOCK_FRAME_LAST_LIST_INDEX + 1)

class nsBodyFrame : public nsBlockFrame,
                    public nsIAbsoluteItems
{
public:
  friend nsresult NS_NewBodyFrame(nsIFrame*& aResult, PRUint32 aFlags);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom*& aListName) const;

  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

#ifdef NS_DEBUG
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
#endif

  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter* aFilter) const;
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  // nsIAbsoluteItems
  NS_IMETHOD  AddAbsoluteItem(nsAbsoluteFrame* aAnchorFrame);
  NS_IMETHOD  RemoveAbsoluteItem(nsAbsoluteFrame* aAnchorFrame);

protected:
  static nsIAtom* gAbsoluteAtom;

  nsBodyFrame();
  ~nsBodyFrame();

  void ReflowAbsoluteItems(nsIPresContext& aPresContext,
                           const nsHTMLReflowState& aReflowState);

  nsIView* CreateAbsoluteView(nsIStyleContext* aStyleContext) const;

  void TranslatePoint(nsIFrame* aFrameFrom, nsPoint& aPoint) const;

  void ComputeAbsoluteFrameBounds(nsIFrame*                aAnchorFrame,
                                  const nsHTMLReflowState& aState,
                                  const nsStylePosition*   aPosition,
                                  nsRect&                  aRect) const;

  void AddAbsoluteFrame(nsIFrame* aFrame);
  PRBool IsAbsoluteFrame(nsIFrame* aFrame);

private:
  nsSpaceManager* mSpaceManager;
  nsVoidArray     mAbsoluteItems;
  nsIFrame*       mAbsoluteFrames;  // additional named child list

#ifdef NS_DEBUG
  struct BandData : public nsBandData {
    // Trapezoids used during band processing
    nsBandTrapezoid data[12];

    // Bounding rect of available space between any left and right floaters
    nsRect          availSpace;

    BandData() {
      size = 12;
      trapezoids = data;
    }

    /**
     * Computes the bounding rect of the available space, i.e. space
     * between any left and right floaters Uses the current trapezoid
     * data, see nsISpaceManager::GetBandData(). Also updates member
     * data "availSpace".
     */
    void ComputeAvailSpaceRect();
  };
#endif
};

#endif /* nsBodyFrame_h___ */
