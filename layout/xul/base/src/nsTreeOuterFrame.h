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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsTableOuterFrame.h"
#include "nsISelfScrollingFrame.h"
#include "nsITreeFrame.h"

class nsTreeOuterFrame : public nsTableOuterFrame, public nsISelfScrollingFrame
{
public:
  friend nsresult NS_NewTreeOuterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus);

  NS_IMETHOD     Reflow(nsIPresContext*          aPresContext,
							         nsHTMLReflowMetrics&     aDesiredSize,
							         const nsHTMLReflowState& aReflowState,
							         nsReflowStatus&          aStatus);

  NS_IMETHOD     Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow);

  NS_IMETHOD AdjustZeroWidth();

  NS_DECL_ISUPPORTS

  NS_IMETHOD FixBadReflowState(const nsHTMLReflowState& aParentReflowState,
                               nsHTMLReflowState& aChildReflowState);

  NS_IMETHOD ScrollByLines(nsIPresContext* aPresContext, PRInt32 lines);
  NS_IMETHOD CollapseScrollbar(nsIPresContext* aPresContext, PRBool aHide);
  NS_IMETHOD ScrollByPages(nsIPresContext* aPresContext, PRInt32 pages);

protected:
  nsTreeOuterFrame();
  virtual ~nsTreeOuterFrame();

  nsITreeFrame* FindTreeFrame(nsIPresContext* aPresContext);

protected: // Data Members
  
}; // class nsTreeOuterFrame
