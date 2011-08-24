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

#ifndef nsHTMLButtonControlFrame_h___
#define nsHTMLButtonControlFrame_h___

#include "nsCOMPtr.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"

#include "nsStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsISupports.h"
#include "nsStyleConsts.h"
#include "nsIComponentManager.h"
#include "nsButtonFrameRenderer.h"

class nsRenderingContext;
class nsPresContext;

class nsHTMLButtonControlFrame : public nsHTMLContainerFrame,
                                 public nsIFormControlFrame 
{
public:
  nsHTMLButtonControlFrame(nsStyleContext* aContext);
  ~nsHTMLButtonControlFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);

  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual PRBool IsContainingBlock() const;
  
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);

  virtual nsStyleContext* GetAdditionalStyleContext(PRInt32 aIndex) const;
  virtual void SetAdditionalStyleContext(PRInt32 aIndex, 
                                         nsStyleContext* aStyleContext);
 
  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList);

  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);

  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  virtual nsIAtom* GetType() const;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("HTMLButtonControl"), aResult);
  }
#endif

  virtual PRBool HonorPrintBackgroundSettings() { return PR_FALSE; }

  // nsIFormControlFrame
  void SetFocus(PRBool aOn, PRBool aRepaint);
  virtual nsresult SetFormProperty(nsIAtom* aName, const nsAString& aValue);
  virtual nsresult GetFormProperty(nsIAtom* aName, nsAString& aValue) const; 

  // Inserted child content gets its frames parented by our child block
  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsHTMLContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

protected:
  virtual PRBool IsInput() { return PR_FALSE; }
  void ReflowButtonContents(nsPresContext* aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsIFrame* aFirstKid,
                            nsMargin aFocusPadding,
                            nsReflowStatus& aStatus);

  PRIntn GetSkipSides() const;
  nsButtonFrameRenderer mRenderer;
};

#endif




