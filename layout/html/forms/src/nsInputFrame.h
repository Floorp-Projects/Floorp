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

#ifndef nsInputFrame_h___
#define nsInputFrame_h___

#include "nsHTMLContainer.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"

class nsIView;
class nsIPresContext;

struct nsInputWidgetData {
  PRInt32 arg1;
  PRInt32 arg2;
  PRInt32 arg3;
  PRInt32 arg4;
};

/**
  * Enumeration of possible mouse states used to detect mouse clicks
  */
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseDown,
  eMouseUp
};

struct nsInputDimensionSpec
{
  nsIAtom* mColSizeAttr;
  PRBool   mColSizeAttrInPixels;
  nsIAtom* mColValueAttr;
  PRInt32  mColDefaultSize;
  PRBool   mColDefaultSizeInPixels;
  nsIAtom* mRowSizeAttr;
  PRInt32  mRowDefaultSize;

  nsInputDimensionSpec(nsIAtom* aColSizeAttr, PRBool aColSizeAttrInPixels, 
                       nsIAtom* aColValueAttr, PRInt32 aColDefaultSize,
                       PRBool aColDefaultSizeInPixels,
                       nsIAtom* aRowSizeAttr, PRInt32 aRowDefaultSize)
                       : mColSizeAttr(aColSizeAttr), mColSizeAttrInPixels(aColSizeAttrInPixels),
                         mColValueAttr(aColValueAttr), mColDefaultSize(aColDefaultSize),
                         mColDefaultSizeInPixels(aColDefaultSizeInPixels),
                         mRowSizeAttr(aRowSizeAttr), mRowDefaultSize(aRowDefaultSize)
  {
  }

};

/** 
  * nsInputFrame is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsInputFrame : public nsLeafFrame {
  typedef nsLeafFrame super;
public:
  /**
    * Main constructor
    * @param aContent the content representing this frame
    * @param aIndexInParent the position in the parent frame's children which this frame occupies
    * @param aParentFrame the parent frame
    */
  nsInputFrame(nsIContent* aContent,
               PRInt32 aIndexInParent,
               nsIFrame* aParentFrame);

  static PRInt32 CalculateSize (nsIPresContext* aPresContext, nsInputFrame* aFrame,
                                const nsSize& aCSSSize, nsInputDimensionSpec& aDimensionSpec, 
                                nsSize& aBounds, PRBool& aWidthExplicit, 
                                PRBool& aHeightExplicit, PRInt32& aRowSize);
  // nsLeafFrame overrides

  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD  SetRect(const nsRect& aRect);

  virtual PRBool IsHidden();

  /**
    * Draw this frame within the context of a presentation context and rendering context
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::ResizeReflow
    */
  NS_IMETHOD ResizeReflow(nsIPresContext*  aCX,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize&    aMaxSize,
                          nsSize*          aMaxElementSize,
                          ReflowStatus&    aStatus);

  // new behavior

  /**
    * Get the class id of the widget associated with this frame
    * @return the class id
    */
  virtual const nsIID& GetCID(); 

  /**
    * Get the interface id of widget associated with this frame
    * @return the interface id
    */
  virtual const nsIID& GetIID(); 

  PRInt32 GetDefaultPadding() const { return 40; }
  virtual PRInt32 GetPadding() const;
  /**
    * Get the widget associated with this frame
    * @param aView the view associated with the frame. It is a convience parm.
    * @param aWidget the address of address of where the widget will be placed.
    * This method doses an AddRef on the widget.
    */
  virtual nsresult GetWidget(nsIView* aView, nsIWidget** aWidget);

  /**
    * Respond to a mouse click (e.g. mouse enter, mouse down, mouse up)
    */
  virtual void MouseClicked(nsIPresContext* aPresContext) {}

  /**
    * Perform opertations after the widget associated with this frame has been
    * created.
    */
  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);  

  /**
    * Perform opertations before the widget associated with this frame has been
    * created.
    */
  virtual nsInputWidgetData* GetWidgetInitData();  

  static PRInt32 GetTextSize(nsIPresContext& aContext, nsIFrame* aFrame,
                             const nsString& aString, nsSize& aSize);
  static PRInt32 GetTextSize(nsIPresContext& aContext, nsIFrame* aFrame,
                             PRInt32 aNumChars, nsSize& aSize);

protected:

  virtual ~nsInputFrame();

  /**
    * Return PR_TRUE if the bounds of this frame have been set
    */
  PRBool BoundsAreSet();

 /**
    * Get the size that this frame would occupy without any constraints
    * @param aPresContext the presentation context
    * @param aDesiredSize the size desired by this frame, to be set by this method
    * @param aMaxSize the maximum size available for this frame
    */
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize& aMaxSize);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsSize& aMaxSize,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

  nsFont& GetFont(nsIPresContext* aPresContext);

   /**
    * Get the width and height of this control based on CSS 
    * @param aPresContext the presentation context
    * @param aMaxSize the maximum size that this frame can have
    * @param aSize the size that this frame wants, set by this method. values of -1 
    * for aSize.width or aSize.height indicate unset values.
    */
  void GetStyleSize(nsIPresContext& aContext, const nsSize& aMaxSize, nsSize& aSize);

  nsSize       mCacheBounds;
  nsMouseState mLastMouseState;
};

#endif

