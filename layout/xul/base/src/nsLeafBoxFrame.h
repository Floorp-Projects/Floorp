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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef nsLeafBoxFrame_h___
#define nsLeafBoxFrame_h___

#include "nsLeafFrame.h"
#include "nsBox.h"

class nsAccessKeyInfo;

class nsLeafBoxFrame : public nsLeafFrame
{
public:

  friend nsresult NS_NewLeafBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIBox frame interface
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize);
  NS_IMETHOD GetFlex(nsBoxLayoutState& aState, nscoord& aFlex);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aState, nscoord& aAscent);
  NS_IMETHOD NeedsRecalc();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD CharacterDataChanged(nsPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  PRBool          aAppend);

  NS_IMETHOD DidReflow(nsPresContext*           aPresContext,
                       const nsHTMLReflowState*  aReflowState,
                       nsDidReflowStatus         aStatus);

  NS_IMETHOD  Init(nsPresContext*  aPresContext,
               nsIContent*      aContent,
               nsIFrame*        aParent,
               nsStyleContext*  aContext,
               nsIFrame*        asPrevInFlow);

  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext,
                          const nsPoint& aPoint,
                          nsFramePaintLayer aWhichLayer,    
                          nsIFrame**     aFrame);
   
  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  NS_IMETHOD GetMouseThrough(PRBool& aMouseThrough);
  virtual PRBool ComputesOwnOverflowArea() { return PR_FALSE; }

protected:

  virtual PRBool HasStyleChange();
  virtual void SetStyleChangeFlag(PRBool aDirty);

  virtual PRBool GetWasCollapsed(nsBoxLayoutState& aState);
  virtual void SetWasCollapsed(nsBoxLayoutState& aState, PRBool aWas);

  NS_IMETHOD DoLayout(nsBoxLayoutState& aState);

#ifdef DEBUG_LAYOUT
  virtual void GetBoxName(nsAutoString& aName);
#endif

  virtual void GetDesiredSize(nsPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize) {}

 nsLeafBoxFrame(nsIPresShell* aShell);

protected:
  eMouseThrough mMouseThrough;

private:

 void UpdateMouseThrough();


}; // class nsLeafBoxFrame

#endif /* nsLeafBoxFrame_h___ */
