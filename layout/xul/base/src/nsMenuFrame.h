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

//
// nsMenuFrame
//

#ifndef nsMenuFrame_h__
#define nsMenuFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

#include "nsAreaFrame.h"
#include "nsFrameList.h"

nsresult NS_NewMenuFrame(nsIFrame** aResult) ;


class nsMenuFrame : public nsAreaFrame
{
public:
  nsMenuFrame();

  // The following four methods are all overridden so that the menu children
  // can be stored in a separate list (so that they don't impact reflow of the
  // actual menu item at all).
  NS_IMETHOD FirstChild(nsIAtom*   aListName,
                        nsIFrame** aFirstChild) const;
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);

  // Overridden to prevent events from ever going to children of the menu.
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, 
                              nsIFrame**     aFrame);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);

  // Reflow methods
  NS_IMETHOD Reflow(nsIPresContext&   aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  void ToggleMenuState();

protected:
  void GetMenuChildrenElement(nsIContent** aResult);

protected:
  nsFrameList mPopupFrames;
  PRBool mMenuOpen;
  PRBool mOnMenuBar;

}; // class nsMenuFrame

#endif
