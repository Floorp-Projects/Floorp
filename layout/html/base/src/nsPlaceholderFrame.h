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
#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "nsFrame.h"
#include "nsIInlineReflow.h"

// Implementation of a frame that's used as a placeholder for an anchored item

class nsPlaceholderFrame : public nsFrame, public nsIInlineReflow {
public:
  /**
   * Create a new placeholder frame
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent,
                           nsIFrame*   aAnchoredItem = nsnull);

  // Returns the associated anchored item
  nsIFrame*   GetAnchoredItem() const {return mAnchoredItem;}
  void        SetAnchoredItem(nsIFrame* aAnchoredItem) {mAnchoredItem = aAnchoredItem;}

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFrame overrides
  NS_IMETHOD FindTextRuns(nsLineLayout&  aLineLayout,
                          nsIReflowCommand* aReflowCommand);
  NS_IMETHOD InlineReflow(nsLineLayout&     aLineLayout,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState);
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD  ContentReplaced(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aOldChild,
                              nsIContent*     aNewChild,
                              PRInt32         aIndexInParent);
  NS_IMETHOD  ContentChanged(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  nsIFrame* mAnchoredItem;

  // Constructor. Takes as arguments the content object and the Frame for
  // the content parent
  nsPlaceholderFrame(nsIContent* aContent, nsIFrame* aParent, nsIFrame* aAnchoredItem);
  virtual ~nsPlaceholderFrame();
};

#endif /* nsPlaceholderFrame_h___ */
