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
#ifndef nsAbsoluteFrame_h___
#define nsAbsoluteFrame_h___

#include "nsFrame.h"

// Implementation of a frame that's used as a placeholder for an absolutely
// positioned frame
class nsAbsoluteFrame : public nsFrame {
public:
  /**
   * Create a placeholder for an absolutely positioned frame. Also creates
   * the absolutely positioned frame itself
   *
   * @see #GetAbsoluteFrame()
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // Returns the associated anchored item
  nsIFrame*   GetAbsoluteFrame() const {return mFrame;}

  // nsIFrame overrides
  NS_IMETHOD  Reflow(nsIPresContext*      aPresContext,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsReflowStatus&      aStatus);

  NS_IMETHOD  ContentAppended(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer);
  NS_IMETHOD  ContentInserted(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aChild,
                              PRInt32         aIndexInParent);
  NS_IMETHOD  ContentReplaced(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aOldChild,
                              nsIContent*     aNewChild,
                              PRInt32         aIndexInParent);
  NS_IMETHOD  ContentDeleted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);
  NS_IMETHOD  ContentChanged(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  nsIFrame* mFrame;  // the absolutely positioned frame

  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  nsAbsoluteFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsAbsoluteFrame();

  nsIFrame* GetContainingBlock() const;
};

#endif /* nsAbsoluteFrame_h___ */
