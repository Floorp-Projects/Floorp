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
#ifndef nsColumnFrame_h___
#define nsColumnFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIRunaround.h"

struct ColumnReflowState;

class ColumnFrame : public nsHTMLContainerFrame, public nsIRunaround {
public:
  ColumnFrame(nsIContent* aContent,
              PRInt32     aIndexInParent,
              nsIFrame*   aParentFrame);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  virtual ReflowStatus  ResizeReflow(nsIPresContext*  aPresContext,
                                     nsISpaceManager* aSpaceManager,
                                     const nsSize&    aMaxSize,
                                     nsRect&          aDesiredRect,
                                     nsSize*          aMaxElementSize);

  virtual ReflowStatus  IncrementalReflow(nsIPresContext*  aPresContext,
                                          nsISpaceManager* aSpaceManager,
                                          const nsSize&    aMaxSize,
                                          nsRect&          aDesiredRect,
                                          nsReflowCommand& aReflowCommand);

  virtual void ContentAppended(nsIPresShell* aShell,
                               nsIPresContext* aPresContext,
                               nsIContent* aContainer);

  virtual nsIFrame* CreateContinuingFrame(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent);

  // Debugging
  virtual void  ListTag(FILE* out = stdout) const;

protected:
  ~ColumnFrame();

  virtual PRIntn GetSkipSides() const;

  nscoord GetTopMarginFor(nsIPresContext* aCX,
                          ColumnReflowState& aState,
                          nsStyleMolecule* aKidMol);

  void PlaceChild(nsIPresContext*    aPresContext,
                  ColumnReflowState& aState,
                  nsIFrame*          aKidFrame,
                  const nsRect&      aKidRect,
                  nsStyleMolecule*   aKidMol,
                  nsSize*            aMaxElementSize,
                  nsSize&            aKidMaxElementSize);

  PRBool        ReflowMappedChildren(nsIPresContext*    aPresContext,
                                     ColumnReflowState& aState,
                                     nsSize*            aMaxElementSize);

  PRBool        PullUpChildren(nsIPresContext*    aPresContext,
                               ColumnReflowState& aState,
                               nsSize*            aMaxElementSize);

  ReflowStatus  ReflowUnmappedChildren(nsIPresContext*    aPresContext,
                                       ColumnReflowState& aState,
                                       nsSize*            aMaxElementSize);
};

#endif /* nsColumnFrame_h___ */
