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

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

#include "nsTextControlFrame.h"
#include "nsFormControlHelper.h"
typedef   nsTextControlFrame nsNewFrame;

class nsIPresState;
class nsISupportsArray;

class nsFileControlFrame : public nsAreaFrame,
                           public nsIFormControlFrame,
                           public nsIDOMMouseListener,
                           public nsIAnonymousContentCreator
{
public:
  nsFileControlFrame();
  virtual ~nsFileControlFrame();

  // XXX Hack so we can squirrel away the pres context pointer
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow) {
    mPresContext = aPresContext;
    return nsAreaFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  }

  virtual void MouseClicked(nsPresContext* aPresContext) {}

  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

      // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAString& aValue); 
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD OnContentReset();

  NS_IMETHOD Reflow(nsPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual void RemovedAsPrimaryFrame(nsPresContext* aPresContext);

  NS_IMETHOD Destroy(nsPresContext *aPresContext);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight) { return NS_OK; };
  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);
  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  NS_IMETHOD     GetName(nsAString* aName);
  NS_IMETHOD_(PRInt32) GetFormControlType() const;
  void           SetFocus(PRBool aOn, PRBool aRepaint);
  void           ScrollIntoView(nsPresContext* aPresContext);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(nsPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  // from nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsPresContext* aPresContext,
                                    nsISupportsArray& aChildList);
  NS_IMETHOD CreateFrameFor(nsPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame) { if (aFrame) *aFrame = nsnull; return NS_ERROR_FAILURE; }


  // mouse events when out browse button is pressed
  /**
  * Processes a mouse down event
  * @param aMouseEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse up event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent); // we only care when the button is clicked

  /**
   * Processes a mouse click event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   *
   */
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse enter event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  /**
   * Processes a mouse leave event
   * @param aMouseEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

protected:
  virtual PRIntn GetSkipSides() const;

  /**
   * The text frame (populated on initial reflow).
   * @see nsFileControlFrame::Reflow
   */
  nsNewFrame* mTextFrame;
  /**
   * The text box input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mTextContent;
  /**
   * The browse button input.
   * @see nsFileControlFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mBrowse;
  /**
   * The current value, stored during those rare in-between periods where the
   * file frame is there but the input frame is not.
   */
  nsString*           mCachedState;
  /**
   * The current pres context.
   * XXX Hack: pres context needed by function MouseClick() and SetFocus()
   */
  nsPresContext*     mPresContext;  // weak reference

private:
  /**
   * Find the first text frame child (first frame child whose content has input
   * type=text) of a frame.
   * XXX this is an awfully complicated implementation of something we could
   * likely do by just doing GetPrimaryFrame on mTextContent
   *
   * @param aPresContext the current pres context
   * @param aStart the parent frame to search children of
   * @return the text control frame, or null if not found
   */
  nsNewFrame* GetTextControlFrame(nsPresContext* aPresContext,
                                  nsIFrame* aStart);

  /**
   * Copy an attribute from file content to text and button content.
   * @param aNameSpaceID namespace of attr
   * @param aAttribute attribute atom
   * @param aWhichControls which controls to apply to (SYNC_TEXT or SYNC_FILE
   *        or SYNC_BOTH)
   */
  void SyncAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                PRBool aWhichControls);

  /**
   * We call this when we are being destroyed or removed from the PFM.
   * @param aPresContext the current pres context
   */
  void PreDestroy(nsPresContext* aPresContext);

  NS_IMETHOD_(nsrefcnt) AddRef() { return 1; }
  NS_IMETHOD_(nsrefcnt) Release() { return 1; }

  PRBool mDidPreDestroy; // has PreDestroy been called
};

#endif


