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
#ifndef nsFrame_h___
#define nsFrame_h___

#include "nsIFrame.h"
#include "nsRect.h"


// Implementation of a simple frame with no children and that isn't splittable
class nsFrame : public nsIFrame
{
public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           PRInt32     aIndexInParent,
                           nsIFrame*   aParent);

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // Overloaded new operator. Initializes the memory to 0
  void* operator new(size_t size);

  NS_IMETHOD  DeleteFrame();

  NS_IMETHOD  GetContent(nsIContent*& aContent) const;
  NS_IMETHOD  GetIndexInParent(PRInt32& aIndexInParent) const;
  NS_IMETHOD  SetIndexInParent(PRInt32 aIndexInParent);

  NS_IMETHOD  GetStyleContext(nsIPresContext* aContext, nsIStyleContext*& aStyleContext);
  NS_IMETHOD  SetStyleContext(nsIStyleContext* aContext);

  // Get the style struct associated with this frame
  NS_IMETHOD  GetStyleData(const nsIID& aSID, nsStyleStruct*& aStyleStruct);


  // Geometric and content parent
  NS_IMETHOD  GetContentParent(nsIFrame*& aParent) const;
  NS_IMETHOD  SetContentParent(const nsIFrame* aParent);
  NS_IMETHOD  GetGeometricParent(nsIFrame*& aParent) const;
  NS_IMETHOD  SetGeometricParent(const nsIFrame* aParent);

  // Bounding rect
  NS_IMETHOD  GetRect(nsRect& aRect) const;
  NS_IMETHOD  GetOrigin(nsPoint& aPoint) const;
  NS_IMETHOD  GetSize(nsSize& aSize) const;
  NS_IMETHOD  SetRect(const nsRect& aRect);
  NS_IMETHOD  MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight);

  // Child frame enumeration
  NS_IMETHOD  ChildCount(PRInt32& aChildCount) const;
  NS_IMETHOD  ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const;
  NS_IMETHOD  IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const;
  NS_IMETHOD  FirstChild(nsIFrame*& aFirstChild) const;
  NS_IMETHOD  NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const;
  NS_IMETHOD  PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const;
  NS_IMETHOD  LastChild(nsIFrame*& aLastChild) const;

  NS_IMETHOD  Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect);

  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus);

  NS_IMETHOD  GetCursorAt(nsIPresContext& aPresContext,
                          const nsPoint&  aPoint,
                          nsIFrame**      aFrame,
                          PRInt32&        aCursor);

  // Resize reflow methods
  NS_IMETHOD  ResizeReflow(nsIPresContext*  aPresContext,
                           nsReflowMetrics& aDesiredSize,
                           const nsSize&    aMaxSize,
                           nsSize*          aMaxElementSize,
                           ReflowStatus&    aStatus);

  NS_IMETHOD  JustifyReflow(nsIPresContext* aPresContext,
                            nscoord         aAvailableSpace);

  // Incremental reflow methods
  NS_IMETHOD  IncrementalReflow(nsIPresContext*  aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize&    aMaxSize,
                                nsReflowCommand& aReflowCommand,
                                ReflowStatus&    aStatus);
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

  NS_IMETHOD  GetReflowMetrics(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aMetrics);

  // Flow member functions
  NS_IMETHOD  IsSplittable(SplittableType& aIsSplittable) const;
  NS_IMETHOD  CreateContinuingFrame(nsIPresContext* aPresContext,
                                    nsIFrame*       aParent,
                                    nsIFrame*&      aContinuingFrame);

  NS_IMETHOD  GetPrevInFlow(nsIFrame*& aPrevInFlow) const;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*);
  NS_IMETHOD  GetNextInFlow(nsIFrame*& aNextInFlow) const;
  NS_IMETHOD  SetNextInFlow(nsIFrame*);

  NS_IMETHOD  AppendToFlow(nsIFrame* aAfterFrame);
  NS_IMETHOD  PrependToFlow(nsIFrame* aAfterFrame);
  NS_IMETHOD  RemoveFromFlow();
  NS_IMETHOD  BreakFromPrevFlow();
  NS_IMETHOD  BreakFromNextFlow();

  // Associated view object
  NS_IMETHOD  GetView(nsIView*& aView) const;
  NS_IMETHOD  SetView(nsIView* aView);

  // Find the first geometric parent that has a view
  NS_IMETHOD  GetParentWithView(nsIFrame*& aParent) const;

  // Returns the offset from this frame to the closest geometric parent that
  // has a view. Also returns the containing view, or null in case of error
  NS_IMETHOD  GetOffsetFromView(nsPoint& aOffset, nsIView*& aView) const;

  // Returns the closest geometric parent that has a view which has a
  // a window.
  NS_IMETHOD  GetWindow(nsIWidget*&) const;

  // Sibling pointer used to link together frames
  NS_IMETHOD  GetNextSibling(nsIFrame*& aNextSibling) const;
  NS_IMETHOD  SetNextSibling(nsIFrame* aNextSibling);

  // Debugging
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD  ListTag(FILE* out = stdout) const;
  NS_IMETHOD  VerifyTree() const;

protected:
  // Constructor. Takes as arguments the content object, the index in parent,
  // and the Frame for the content parent
  nsFrame(nsIContent* aContent,
          PRInt32     aIndexInParent,
          nsIFrame*   aParent);

  virtual ~nsFrame();

  nsRect           mRect;
  nsIContent*      mContent;
  nsIStyleContext* mStyleContext;
  PRInt32          mIndexInParent;
  nsIFrame*        mContentParent;
  nsIFrame*        mGeometricParent;
  nsIFrame*        mNextSibling;  // singly linked list of frames

private:
  nsIView*         mView;  // must use accessor member functions

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif /* nsFrame_h___ */
