/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsTableRowGroupFrame.h"
#include "nsVoidArray.h"
#include "nsIScrollbarListener.h"

class nsTreeFrame;
class nsCSSFrameConstructor;

class nsTreeRowGroupFrame : public nsTableRowGroupFrame, public nsIScrollbarListener
{
public:
  friend nsresult NS_NewTreeRowGroupFrame(nsIFrame** aNewFrame);

  virtual PRBool ExcludeFrameFromReflow(nsIFrame* aFrame);

  void SetScrollbarFrame(nsIFrame* aFrame);
  void SetFrameConstructor(nsCSSFrameConstructor* aFrameConstructor) { mFrameConstructor = aFrameConstructor; };

  void MakeLazy() { mIsLazy = PR_TRUE; };
  PRBool IsLazy() { return mIsLazy; };

  NS_IMETHOD  TreeAppendFrames(nsIFrame*       aFrameList);

  NS_IMETHOD  TreeInsertFrames(nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList);

  NS_IMETHOD  GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);
  void PaintChildren(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer);

  PRBool ContinueReflow(nscoord y, nscoord height);

  PRBool IsFull() { return mIsFull; };

  // Responses to changes
  void OnContentAdded(nsIPresContext& aPresContext);

  virtual nsIFrame* GetFirstFrame();
  virtual nsIFrame* GetLastFrame();
  virtual void GetNextFrame(nsIFrame* aFrame, nsIFrame** aResult);
  virtual PRBool RowsDesireExcessSpace() { return PR_FALSE; };
  virtual PRBool RowGroupDesiresExcessSpace();

  NS_DECL_ISUPPORTS

  NS_IMETHOD PositionChanged(nsIPresContext& aPresContext, PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD PagedUpDown();

protected:
  nsTreeRowGroupFrame();
  virtual ~nsTreeRowGroupFrame();

  virtual PRBool RowGroupReceivesExcessSpace();
  
  void DestroyRows(nsIPresContext& aPresContext, PRInt32& rowsToLose);
  void ReverseDestroyRows(nsIPresContext& aPresContext, PRInt32& rowsToLose);

  void ConstructContentChain(PRInt32 aOldIndex, PRInt32 aNewIndex, nsIContent* aContent);

  NS_IMETHOD     ReflowBeforeRowLayout(nsIPresContext&      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason);

  virtual nsIFrame* GetFirstFrameForReflow(nsIPresContext& aPresContext);
  virtual void GetNextFrameForReflow(nsIPresContext& aPresContext, nsIFrame* aFrame, nsIFrame** aResult);

  void LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult);

protected: // Data Members
  nsIFrame* mTopFrame; // The current topmost frame in the view.
  nsIFrame* mBottomFrame; // The current bottom frame in the view.

  PRBool mIsLazy; // Whether or not we're a lazily instantiated beast
  PRBool mIsFull; // Whether or not we have any more room.

  nsIFrame* mScrollbar; // Our scrollbar.
  
  nsCSSFrameConstructor* mFrameConstructor; // We don't own this. (No addref/release allowed, punk.)
}; // class nsTreeRowGroupFrame
