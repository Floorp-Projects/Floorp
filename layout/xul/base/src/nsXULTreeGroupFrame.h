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
 *   Mike Pinkerton (pinkerton@netscape.com)
 */

#ifndef NSXULTREEGROUPFRAME
#define NSXULTREEGROUPFRAME


#include "nsBoxFrame.h"
#include "nsIXULTreeSlice.h"

class nsCSSFrameConstructor;
class nsXULTreeOuterGroupFrame;
class nsTreeItemDragCapturer;

class nsXULTreeGroupFrame : public nsBoxFrame, public nsIXULTreeSlice
{
public:
  NS_DECL_ISUPPORTS

  friend nsresult NS_NewXULTreeGroupFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame, 
                                          PRBool aIsRoot = PR_FALSE,
                                          nsIBoxLayout* aLayoutManager = nsnull);

protected:
  nsXULTreeGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull);
  virtual ~nsXULTreeGroupFrame();

  void LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult);

public:
  void InitGroup(nsCSSFrameConstructor* aFC, nsIPresContext* aContext, nsXULTreeOuterGroupFrame* aOuterFrame) 
  {
    mFrameConstructor = aFC;
    mPresContext = aContext;
    mOuterFrame = aOuterFrame;
  }

    // overridden for d&d setup and feedback
  NS_IMETHOD Init ( nsIPresContext*  aPresContext, nsIContent* aContent,
                      nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow) ;
  NS_IMETHOD Paint(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect, nsFramePaintLayer aWhichLayer, PRUint32 aFlags = 0);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext, nsIContent* aChild,
                                 PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aModType, PRInt32 aHint) ;

  nsXULTreeOuterGroupFrame* GetOuterFrame() { return mOuterFrame; };
  nsIBox* GetFirstTreeBox(PRBool* aCreated = nsnull);
  nsIBox* GetNextTreeBox(nsIBox* aBox, PRBool* aCreated = nsnull);

  nsIFrame* GetFirstFrame();
  nsIFrame* GetNextFrame(nsIFrame* aCurrFrame);
  nsIFrame* GetLastFrame();
  
  NS_IMETHOD TreeAppendFrames(nsIFrame*       aFrameList);

  NS_IMETHOD TreeInsertFrames(nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList);

  NS_IMETHOD Redraw(nsBoxLayoutState& aState,
                    const nsRect*   aDamageRect,
                    PRBool          aImmediate);

  // Responses to changes
  void OnContentInserted(nsIPresContext* aPresContext, nsIFrame* aNextSibling, PRInt32 aIndex);
  void OnContentRemoved(nsIPresContext* aPresContext, nsIFrame* aChildFrame, PRInt32 aIndex, PRInt32& aOnScreenRowCount);

  // nsIXULTreeSlice
  NS_IMETHOD IsOutermostFrame(PRBool* aResult) { *aResult = PR_FALSE; return NS_OK; };
  NS_IMETHOD IsGroupFrame(PRBool* aResult) { *aResult = PR_TRUE; return NS_OK; };
  NS_IMETHOD IsRowFrame(PRBool* aResult) { *aResult = PR_FALSE; return NS_OK; };
  NS_IMETHOD GetOnScreenRowCount(PRInt32* aCount);
  
  // nsIBox
  NS_IMETHOD NeedsRecalc();

  virtual nscoord GetAvailableHeight() { return mAvailableHeight; };
  void SetAvailableHeight(nscoord aHeight) { mAvailableHeight = aHeight; };

  virtual nscoord GetYPosition() { return 0; };
  PRBool ContinueReflow(nscoord height);

  void DestroyRows(PRInt32& aRowsToLose);
  void ReverseDestroyRows(PRInt32& aRowsToLose);
  void GetFirstRowContent(nsIContent** aResult);

  void SetContentChain(nsISupportsArray* aContentChain);
  void InitSubContentChain(nsXULTreeGroupFrame* aRowGroupFrame);

protected: 

    // handle drawing the drop feedback
  void PaintDropFeedback ( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                             PRBool aPaintSorted ) ;
  void PaintSortedDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext, float & inP2T ) ;
  void PaintOnContainerDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext, 
                                        nsIPresContext* inPresContext, float & inP2T ) ;
  void PaintInBetweenDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext, 
                                        nsIPresContext* inPresContext, float & inP2T ) ;

    // helpers for drop feedback
  PRInt32 FindIndentation ( nsIPresContext* inPresContext, nsIFrame* inStartFrame ) const ;
  void FindFirstChildTreeItemFrame ( nsIPresContext* inPresContext, nsIFrame** outChild ) const ;
  PRBool IsOpenContainer ( ) const ;
  nscolor GetColorFromStyleContext ( nsIPresContext* inPresContext, nsIAtom* inAtom, 
                                       nscolor inDefaultColor ) ;
  static void ForceDrawFrame ( nsIPresContext* aPresContext, nsIFrame * aFrame ) ;

  nsCSSFrameConstructor* mFrameConstructor; // We don't own this. (No addref/release allowed, punk.)
  nsIPresContext* mPresContext;
  nsXULTreeOuterGroupFrame* mOuterFrame;
  nscoord mAvailableHeight;
  nsIFrame* mTopFrame;
  nsIFrame* mBottomFrame;
  nsIFrame* mLinkupFrame;
  nsISupportsArray* mContentChain; // Our content chain
  PRInt32 mOnScreenRowCount;
  
  // -- members for drag and drop --
  
    // our event capturer registered with the content model. See the discussion
    // in Init() for why this is a weak ref.
  nsTreeItemDragCapturer* mDragCapturer;

    // only used during drag and drop for drop feedback. These are not
    // guaranteed to be meaningful when no drop is underway.
  PRInt32 mYDropLoc;
  PRPackedBool mDropOnContainer;

}; // class nsXULTreeGroupFrame


#endif
