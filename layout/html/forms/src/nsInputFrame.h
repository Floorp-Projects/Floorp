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

#include "nsHTMLTagContent.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"

class nsIView;

/**
  * Enumeration of possible mouse states used to detect mouse clicks
  */
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseDown,
  eMouseUp
};

/** 
  * nsInputFrame is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsInputFrame : public nsLeafFrame {
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

  // nsLeafFrame overrides

  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);
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

  // New Behavior

  /**
    * Get the class id of the widget associated with this frame
    * @return the class id
    */
  virtual const nsIID GetCID(); 

  /**
    * Get the interface id of widget associated with this frame
    * @return the interface id
    */
  virtual const nsIID GetIID(); 

  /**
    * Get the widget associated with this frame
    * @param aView the view associated with the frame. It is a convience parm.
    * @param aWidget the address of address of where the widget will be placed.
    * This method doses an AddRef on the widget.
    */
  virtual nsresult GetWidget(nsIView* aView, nsIWidget** aWidget);

  /**
    * Perform opertations after the widget associated with this frame has been
    * fully constructed.
    */
  virtual void InitializeWidget(nsIView *aView) = 0;  

  /**
    * Respond to a mouse click (e.g. mouse enter, mouse down, mouse up)
    */
  virtual void MouseClicked() {}

  /**
    * Perform operations before the widget associated with this frame has been
    * constructed.
    * @param aPresContext the presentation context
    * @param aBounds the bounds of this frame. It will be set by this method.
    */
  virtual void PreInitializeWidget(nsIPresContext* aPresContext, 
	                                 nsSize& aBounds); 

protected:

  virtual ~nsInputFrame();

  /**
    * Get the size that this frame would occupy without any constraints
    * @param aPresContext the presentation context
    * @param aDesiredSize the size desired by this frame, to be set by this method
    * @param aMaxSize the maximum size available for this frame
    */
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize& aMaxSize);
  /**
    * Return PR_TRUE if the bounds of this frame have been set
    */
  PRBool BoundsAreSet();

  nsSize       mCacheBounds;
  nsMouseState mLastMouseState;
};

#endif
