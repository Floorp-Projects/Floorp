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

#include "nsHTMLContainerFrame.h"
#include "nsIAnchoredItems.h"

struct nsBodyReflowState;
class SpaceManager;
struct nsStyleSpacing;

class nsBodyFrame : public nsHTMLContainerFrame, public nsIAnchoredItems {
public:
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD ResizeReflow(nsIPresContext*  aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize&    aMaxSize,
                          nsSize*          aMaxElementSize,
                          ReflowStatus&    aStatus);

  NS_IMETHOD IncrementalReflow(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsReflowCommand& aReflowCommand,
                               ReflowStatus&    aStatus);

  NS_IMETHOD ContentAppended(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer);

  NS_IMETHOD ContentInserted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);

  NS_IMETHOD CreateContinuingFrame(nsIPresContext* aPresContext,
                                   nsIFrame*       aParent,
                                   nsIFrame*&      aContinuingFrame);

  virtual void AddAnchoredItem(nsIFrame*         aAnchoredItem,
                               AnchoringPosition aPosition,
                               nsIFrame*         aContainer);

  virtual void RemoveAnchoredItem(nsIFrame* aAnchoredItem);

  NS_IMETHOD VerifyTree() const;

protected:
  nsBodyFrame(nsIContent* aContent,
              PRInt32     aIndexInParent,
              nsIFrame*   aParentFrame);

  ~nsBodyFrame();

  virtual PRIntn GetSkipSides() const;

private:
  SpaceManager* mSpaceManager;

  void CreateColumnFrame(nsIPresContext* aPresContext);
  nsSize GetColumnAvailSpace(nsIPresContext* aPresContext,
                             nsStyleSpacing* aSpacing,
                             const nsSize&   aMaxSize);
};

#endif /* nsBodyFrame_h___ */
