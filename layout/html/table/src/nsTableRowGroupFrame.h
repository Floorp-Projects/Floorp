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
#ifndef nsTableRowGroupFrame_h__
#define nsTableRowGroupFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

struct RowGroupReflowState;
struct nsStyleMolecule;

/**
 * nsTableRowGroupFrame
 * data structure to maintain information about a single table cell's frame
 *
 * @author  sclark
 */
class nsTableRowGroupFrame : public nsContainerFrame
{
public:
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  virtual void  Paint(nsIPresContext& aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect& aDirtyRect);

  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect);

  ReflowStatus  ResizeReflow(nsIPresContext* aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize&   aMaxSize,
                             nsSize*         aMaxElementSize);

  ReflowStatus  IncrementalReflow(nsIPresContext*  aPresContext,
                                  nsReflowMetrics& aDesiredSize,
                                  const nsSize&    aMaxSize,
                                  nsReflowCommand& aReflowCommand);

  /**
   * @see nsContainerFrame
   */
  virtual nsIFrame* CreateContinuingFrame(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent);

protected:

  nsTableRowGroupFrame(nsIContent* aContent,
                       PRInt32 aIndexInParent,
					             nsIFrame* aParentFrame);

  ~nsTableRowGroupFrame();

  nscoord GetTopMarginFor(nsIPresContext*      aCX,
                          RowGroupReflowState& aState,
                          nsStyleMolecule*     aKidMol);

  void          PlaceChild( nsIPresContext*      aPresContext,
                            RowGroupReflowState& aState,
                            nsIFrame*            aKidFrame,
                            const nsRect&        aKidRect,
                            nsStyleMolecule*     aKidMol,
                            nsSize*              aMaxElementSize,
                            nsSize&              aKidMaxElementSize);

  PRBool        ReflowMappedChildren(nsIPresContext*      aPresContext,
                                     RowGroupReflowState& aState,
                                     nsSize*              aMaxElementSize);

  PRBool        PullUpChildren(nsIPresContext*      aPresContext,
                               RowGroupReflowState& aState,
                               nsSize*              aMaxElementSize);

  ReflowStatus  ReflowUnmappedChildren(nsIPresContext*      aPresContext,
                                       RowGroupReflowState& aState,
                                       nsSize*              aMaxElementSize);

};


#endif
