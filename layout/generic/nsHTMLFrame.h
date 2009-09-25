/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object that goes directly inside the document's scrollbars */

#ifndef nsHTMLFrame_h___
#define nsHTMLFrame_h___


#include "nsHTMLContainerFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsGkAtoms.h"
#include "nsIScrollPositionListener.h"
#include "nsDisplayList.h"
#include "nsAbsoluteContainingBlock.h"

// for focus
#include "nsIScrollableView.h"
#include "nsICanvasFrame.h"

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame
 */
class CanvasFrame : public nsHTMLContainerFrame, 
                    public nsIScrollPositionListener, 
                    public nsICanvasFrame {
public:
  CanvasFrame(nsStyleContext* aContext)
  : nsHTMLContainerFrame(aContext), mDoPaintFocus(PR_FALSE),
    mAbsoluteContainer(nsGkAtoms::absoluteList) {}

  NS_DECL_QUERYFRAME_TARGET(CanvasFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsISupports (nsIScrollPositionListener)
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  virtual void Destroy();

  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsFrameList&    aChildList);
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsFrameList&    aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;
  virtual nsFrameList GetChildList(nsIAtom* aListName) const;

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  virtual PRBool IsContainingBlock() const { return PR_TRUE; }
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsHTMLContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers));
  }

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  void PaintFocus(nsIRenderingContext& aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);
  virtual void ViewPositionDidChange(nsIScrollableView* aScrollable,
                                     nsTArray<nsIWidget::Configuration>* aConfigurations) {}
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);

  // nsICanvasFrame
  NS_IMETHOD SetHasFocus(PRBool aHasFocus);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::canvasFrame
   */
  virtual nsIAtom* GetType() const;

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              PRBool         aForceNormal)
  {
    NS_ASSERTION(!aForceNormal, "No-one should be passing this in here");

    // CanvasFrame keeps overflow container continuations of its child
    // frame in main child list
    nsresult rv = nsContainerFrame::StealFrame(aPresContext, aChild, PR_TRUE);
    if (NS_FAILED(rv)) {
      rv = nsContainerFrame::StealFrame(aPresContext, aChild);
    }
    return rv;
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  NS_IMETHOD GetContentForEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent** aContent);

  nsRect CanvasArea() const;

protected:
  virtual PRIntn GetSkipSides() const;

  // Data members
  PRPackedBool              mDoPaintFocus;
  nsCOMPtr<nsIViewManager>  mViewManager;
  nsAbsoluteContainingBlock mAbsoluteContainer;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

#endif /* nsHTMLFrame_h___ */
