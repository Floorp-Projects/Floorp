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
#ifndef nsHTMLFrameset_h___
#define nsHTMLFrameset_h___

#include "nsHTMLAtoms.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLContainer.h"

class  nsIContent;
struct nsReflowMetrics;
class  nsIFrame;
class  nsIPresContext;
class  nsIRenderingContext;
struct nsRect;
struct nsReflowState;
struct nsSize;
class  nsIAtom;
class  nsIWebShell;

enum nsFramesetUnit {
  eFramesetUnit_Free = 0,
  eFramesetUnit_Percent,
  eFramesetUnit_Pixel
};

struct nsFramesetSpec {
  nsFramesetUnit mUnit;
  nscoord        mValue;
};


/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
class nsHTMLFramesetFrame : public nsHTMLContainerFrame {

public:
  nsHTMLFramesetFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsHTMLFramesetFrame();

  static PRInt32 gMaxNumRowColSpecs;

  void GetSizeOfChild(nsIFrame* aChild, nsSize& aSize);

  void GetSizeOfChildAt(PRInt32 aIndexInParent, nsSize& aSize, nsPoint& aCellIndex);

  static nsHTMLFramesetFrame* GetFramesetParent(nsIFrame* aChild);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

protected:
  void CalculateRowCol(nsIPresContext* aPresContext, nscoord aSize, PRInt32 aNumSpecs, 
                       nsFramesetSpec* aSpecs, nscoord* aValues);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  virtual PRIntn GetSkipSides() const;

  void ParseRowCol(nsIAtom* aAttrType, PRInt32& aNumSpecs, nsFramesetSpec** aSpecs); 

  PRInt32 ParseRowColSpec(nsString& aSpec, PRInt32 aMaxNumValues,
                          nsFramesetSpec* aSpecs);

  PRInt32          mNumRows;
  nsFramesetSpec*  mRowSpecs;  // parsed, non-computed dimensions
  nscoord*         mRowSizes;  // currently computed row sizes 
  PRInt32          mNumCols;
  nsFramesetSpec*  mColSpecs;  // parsed, non-computed dimensions
  nscoord*         mColSizes;  // currently computed col sizes 
};

/*******************************************************************************
 * nsHTMLFrameset
 ******************************************************************************/
class nsHTMLFrameset : public nsHTMLContainer {
public:
 
  virtual nsresult  CreateFrame(nsIPresContext*  aPresContext,
                                nsIFrame*        aParentFrame,
                                nsIStyleContext* aStyleContext,
                                nsIFrame*&       aResult);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);
  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

protected:
  nsHTMLFrameset(nsIAtom* aTag, nsIWebShell* aParentWebShell);
  virtual  ~nsHTMLFrameset();

  friend nsresult NS_NewHTMLFrameset(nsIHTMLContent** aInstancePtrResult,
                                     nsIAtom* aTag, nsIWebShell* aWebShell);
  //friend class nsHTMLFramesetFrame;

  // this is held for a short time until the frame uses it, so it is not ref counted
  nsIWebShell*    mParentWebShell; 
};

#endif
