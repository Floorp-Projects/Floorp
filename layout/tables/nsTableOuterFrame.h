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
#ifndef nsTableOuterFrame_h__
#define nsTableOuterFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

class nsTableFrame;
class nsVoidArray;
struct nsStyleMolecule;
class nsTableCaptionFrame;
struct OuterTableReflowState;

/**
 * nsTableOuterFrame
 * main frame for an nsTable content object.
 * the nsTableOuterFrame contains 0 or more nsTableCaptionFrames, 
 * and a single nsTableFrame psuedo-frame.
 *
 * @author  sclark
 */
class nsTableOuterFrame : public nsContainerFrame
{
public:
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  virtual void  Paint(nsIPresContext& aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect& aDirtyRect);

  ReflowStatus  ResizeReflow(nsIPresContext*  aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize&    aMaxSize,
                             nsSize*          aMaxElementSize);

  ReflowStatus  IncrementalReflow(nsIPresContext*  aPresContext,
                                  nsReflowMetrics& aDesiredSize,
                                  const nsSize&    aMaxSize,
                                  nsReflowCommand& aReflowCommand);

  /**
    * @see nsContainerFrame
    */
  virtual nsIFrame* CreateContinuingFrame(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent);
  /** destructor */
  virtual ~nsTableOuterFrame();

protected:

  /** constructor */
  nsTableOuterFrame(nsIContent* aContent,
                   PRInt32 aIndexInParent,
					         nsIFrame* aParentFrame);

  /** return PR_TRUE if the table needs to be reflowed.  
    * the outer table needs to be reflowed if the table content has changed,
    * or if the combination of table style attributes and max height/width have
    * changed.
    */
  virtual PRBool NeedsReflow(const nsSize& aMaxSize);

  /** returns PR_TRUE if the data obtained from the first reflow pass
    * is cached and still valid (ie, no content or style change notifications.)
    */
  virtual PRBool IsFirstPassValid();

  /** setter for mFirstPassValid. 
    * should be called with PR_FALSE when:
    *   content changes, style changes, or context changes
    */
  virtual void SetFirstPassValid(PRBool aValidState);

  /** create all child frames for this table */
  virtual void CreateChildFrames(nsIPresContext*  aPresContext);

  /** reflow the captions in an infinite space, caching the min/max sizes for each
    */
  virtual ReflowStatus ResizeReflowCaptionsPass1(nsIPresContext*  aPresContext, 
                                                 nsStyleMolecule* aTableStyle);

  /** reflow the top captions in a space constrained by the computed table width
    * and the heigth given to us by our parent.  Top captions are laid down
    * before the inner table.
    */
  virtual ReflowStatus ResizeReflowTopCaptionsPass2(nsIPresContext*  aPresContext,
                                                    nsReflowMetrics& aDesiredSize,
                                                    const nsSize&    aMaxSize,
                                                    nsSize*          aMaxElementSize,
                                                    nsStyleMolecule* aTableStyle);

  /** reflow the bottom captions in a space constrained by the computed table width
    * and the heigth given to us by our parent.  Bottom captions are laid down
    * after the inner table.
    */
  virtual ReflowStatus ResizeReflowBottomCaptionsPass2(nsIPresContext*  aPresContext,
                                                       nsReflowMetrics& aDesiredSize,
                                                       const nsSize&    aMaxSize,
                                                       nsSize*          aMaxElementSize,
                                                       nsStyleMolecule* aTableStyle,
                                                       nscoord          aYOffset);

  nscoord       GetTopMarginFor(nsIPresContext* aCX,
                                OuterTableReflowState& aState,
                                nsStyleMolecule* aKidMol);

  void          PlaceChild( nsIPresContext*    aPresContext,
                            OuterTableReflowState& aState,
                            nsIFrame*          aKidFrame,
                            const nsRect&      aKidRect,
                            nsStyleMolecule*   aKidMol,
                            nsSize*            aMaxElementSize,
                            nsSize&            aKidMaxElementSize);

  PRBool        ReflowMappedChildren(nsIPresContext*        aPresContext,
                                     OuterTableReflowState& aState,
                                     nsSize*                aMaxElementSize);

  PRBool        PullUpChildren(nsIPresContext*        aPresContext,
                               OuterTableReflowState& aState,
                               nsSize*                aMaxElementSize);

  virtual       void SetReflowState(OuterTableReflowState& aState, 
                                    nsIFrame*              aKidFrame);

  virtual nsIFrame::ReflowStatus ReflowChild( nsIFrame*        aKidFrame,
                                              nsIPresContext*  aPresContext,
                                              nsReflowMetrics& aDesiredSize,
                                              const nsSize&    aMaxSize,
                                              nsSize*          aMaxElementSize,
                                              OuterTableReflowState& aState);

 /**
   * Sets the last content offset based on the last child frame. If the last
   * child is a pseudo frame then it sets mLastContentIsComplete to be the same
   * as the last child's mLastContentIsComplete
   */
  //virtual void SetLastContentOffset(const nsIFrame* aLastChild);

  /**
    * See nsContainerFrame::VerifyTree
    */
  virtual void VerifyTree() const;

  /**
    * See nsContainerFrame::PrepareContinuingFrame
    */
  virtual void PrepareContinuingFrame(nsIPresContext*    aPresContext,
                                      nsIFrame*          aParent,
                                      nsTableOuterFrame* aContFrame);

  /**
    */
  virtual void CreateInnerTableFrame(nsIPresContext* aPresContext);


private:
  /** used to keep track of this frame's children */
  nsTableFrame *mInnerTableFrame;
  nsVoidArray * mCaptionFrames;
  nsVoidArray * mBottomCaptions;

  /** used to keep track of min/max caption requirements */
  PRInt32 mMinCaptionWidth;
  PRInt32 mMaxCaptionWidth;

  /** used to cache reflow results so we can optimize out reflow in some circumstances */
  nsReflowMetrics mDesiredSize;
  nsSize mMaxElementSize;

  /** we can skip the first pass on captions if mFirstPassValid is true */
  PRBool mFirstPassValid;

};



#endif
