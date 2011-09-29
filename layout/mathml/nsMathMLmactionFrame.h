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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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

#ifndef nsMathMLmactionFrame_h___
#define nsMathMLmactionFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"
#include "nsIDOMEventListener.h"

//
// <maction> -- bind actions to a subexpression
//

class nsMathMLmactionFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData();

  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList);

  virtual nsresult
  ChildListChanged(PRInt32 aModType);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

private:
  void MouseClick();
  void MouseOver();
  void MouseOut();

  class MouseListener : public nsIDOMEventListener
  {
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER

    MouseListener(nsMathMLmactionFrame* aOwner) : mOwner(aOwner) { };

    nsMathMLmactionFrame* mOwner;
  };

protected:
  nsMathMLmactionFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmactionFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  PRInt32         mActionType;
  PRInt32         mChildCount;
  PRInt32         mSelection;
  nsIFrame*       mSelectedFrame;
  nsString        mRestyle;
  nsCOMPtr<MouseListener> mListener;

  // helper to return the frame for the attribute selection="number"
  nsIFrame* 
  GetSelectedFrame();
};

#endif /* nsMathMLmactionFrame_h___ */
