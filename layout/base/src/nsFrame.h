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

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // Overloaded new operator. Initializes the memory to 0
  void* operator new(size_t size);

  virtual void DeleteFrame();

  virtual nsIContent*   GetContent() const;
  virtual PRInt32       GetIndexInParent() const;
  virtual void          SetIndexInParent(PRInt32 aIndexInParent);

  virtual nsIStyleContext* GetStyleContext(nsIPresContext* aContext);
  virtual void             SetStyleContext(nsIStyleContext* aContext);

  // Geometric and content parent
  virtual nsIFrame*     GetContentParent() const;
  virtual void          SetContentParent(const nsIFrame* aParent);
  virtual nsIFrame*     GetGeometricParent() const;
  virtual void          SetGeometricParent(const nsIFrame* aParent);

  // Bounding rect
  virtual nsRect        GetRect() const;
  virtual void          GetRect(nsRect& aRect) const;
  virtual void          GetOrigin(nsPoint& aPoint) const;
  virtual nscoord       GetWidth() const;
  virtual nscoord       GetHeight() const;
  virtual void          SetRect(const nsRect& aRect);
  virtual void          MoveTo(nscoord aX, nscoord aY);
  virtual void          SizeTo(nscoord aWidth, nscoord aHeight);

  // Child frame enumeration
  virtual PRInt32       ChildCount() const;
  virtual nsIFrame*     ChildAt(PRInt32 aIndex) const;
  virtual PRInt32       IndexOf(const nsIFrame* aChild) const;
  virtual nsIFrame*     FirstChild() const;
  virtual nsIFrame*     NextChild(const nsIFrame* aChild) const;
  virtual nsIFrame*     PrevChild(const nsIFrame* aChild) const;
  virtual nsIFrame*     LastChild() const;

  virtual void          Paint(nsIPresContext&      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect);

  virtual nsEventStatus        HandleEvent(nsIPresContext& aPresContext, 
                                    nsGUIEvent*     aEvent);

  virtual PRInt32       GetCursorAt(nsIPresContext& aPresContext,
                                    const nsPoint& aPoint,
                                    nsIFrame** aFrame);

  // Resize reflow methods
  virtual ReflowStatus  ResizeReflow(nsIPresContext*  aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsSize*          aMaxElementSize);
  virtual void          JustifyReflow(nsIPresContext* aCX,
                                      nscoord         aAvailableSpace);

  // Incremental reflow methods
  virtual ReflowStatus  IncrementalReflow(nsIPresContext*  aPresContext,
                                          nsReflowMetrics& aDesiredSize,
                                          const nsSize&    aMaxSize,
                                          nsReflowCommand& aReflowCommand);
  virtual void ContentAppended(nsIPresShell* aShell,
                               nsIPresContext* aPresContext,
                               nsIContent* aContainer);
  virtual void ContentInserted(nsIPresShell* aShell,
                               nsIPresContext* aPresContext,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInParent);
  virtual void ContentReplaced(nsIPresShell* aShell,
                               nsIPresContext* aPresContext,
                               nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInParent);
  virtual void ContentDeleted(nsIPresShell* aShell,
                              nsIPresContext* aPresContext,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInParent);
  virtual void          GetReflowMetrics(nsIPresContext* aPresContext,
                                         nsReflowMetrics& aMetrics);

  // Flow member functions
  virtual PRBool        IsSplittable() const;
  virtual nsIFrame*     CreateContinuingFrame(nsIPresContext* aPresContext,
                                              nsIFrame*       aParent);
  virtual nsIFrame*     GetPrevInFlow() const;
  virtual void          SetPrevInFlow(nsIFrame*);
  virtual nsIFrame*     GetNextInFlow() const;
  virtual void          SetNextInFlow(nsIFrame*);

  virtual void          AppendToFlow(nsIFrame* aAfterFrame);
  virtual void          PrependToFlow(nsIFrame* aAfterFrame);
  virtual void          RemoveFromFlow();
  virtual void          BreakFromPrevFlow();
  virtual void          BreakFromNextFlow();

  // Associated view object
  virtual nsIView*      GetView() const;
  virtual void          SetView(nsIView* aView);

  // Find the first geometric parent that has a view
  virtual nsIFrame*     GetParentWithView() const;

  // Returns the offset from this frame to the closest geometric parent that
  // has a view. Also returns the containing view, or null in case of error
  virtual nsIView*      GetOffsetFromView(nsPoint& aOffset) const;

  // Returns the closest geometric parent that has a view which has a
  // a window.
  virtual nsIWidget*    GetWindow() const;

  // Sibling pointer used to link together frames
  virtual nsIFrame*     GetNextSibling() const;
  virtual void          SetNextSibling(nsIFrame* aNextSibling);

  // Debugging
  virtual void          List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  virtual void          ListTag(FILE* out = stdout) const;
  virtual void          VerifyTree() const;

  // Get the style struct associated with this frame
  virtual nsStyleStruct* GetStyleData(const nsIID& aSID);


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
