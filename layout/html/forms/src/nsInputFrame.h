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

#include "nsIFormManager.h"
#include "nsHTMLContainer.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"
#include "nsCoord.h"

class nsIView;
class nsIPresContext;
class nsStyleCoord;

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
  nsIAtom*  mColSizeAttr;            // attribute used to determine width
  PRBool    mColSizeAttrInPixels;    // is attribute value in pixels (otherwise num chars)
  nsIAtom*  mColValueAttr;           // attribute used to get value to determine size
                                     //    if not determined above
  nsString* mColDefaultValue;        // default value if not determined above
  nscoord   mColDefaultSize;         // default width if not determined above
  PRBool    mColDefaultSizeInPixels; // is default width in pixels (otherswise num chars)
  nsIAtom*  mRowSizeAttr;            // attribute used to determine height
  nscoord   mRowDefaultSize;         // default height if not determined above

  nsInputDimensionSpec(nsIAtom* aColSizeAttr, PRBool aColSizeAttrInPixels, 
                       nsIAtom* aColValueAttr, nsString* aColDefaultValue,
                       nscoord aColDefaultSize, PRBool aColDefaultSizeInPixels,
                       nsIAtom* aRowSizeAttr, nscoord aRowDefaultSize)
                       : mColSizeAttr(aColSizeAttr), mColSizeAttrInPixels(aColSizeAttrInPixels),
                         mColValueAttr(aColValueAttr), 
                         mColDefaultValue(aColDefaultValue), mColDefaultSize(aColDefaultSize),
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
  typedef nsLeafFrame nsInputFrameSuper;
public:
  /**
    * Main constructor
    * @param aContent the content representing this frame
    * @param aParentFrame the parent frame
    */
  nsInputFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  static nscoord CalculateSize (nsIPresContext* aPresContext, nsInputFrame* aFrame,
                                const nsSize& aCSSSize, nsInputDimensionSpec& aDimensionSpec, 
                                nsSize& aBounds, PRBool& aWidthExplicit, 
                                PRBool& aHeightExplicit, nscoord& aRowSize);
  // nsLeafFrame overrides

  NS_IMETHOD MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD SizeTo(nscoord aWidth, nscoord aHeight);

  /** 
    * Respond to a gui event
    * @see nsIFrame::HandleEvent
    */
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD  SetRect(const nsRect& aRect);

  /**
    * Draw this frame within the context of a presentation context and rendering context
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*      aCX,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  // new behavior

  nsFormRenderingMode GetMode() const;
  /**
   * Return true if the underlying form element is a hidden form element
   */
  PRBool IsHidden();

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
  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);  

  static nscoord GetTextSize(nsIPresContext& aContext, nsInputFrame* aFrame,
                             const nsString& aString, nsSize& aSize);
  static nscoord GetTextSize(nsIPresContext& aContext, nsInputFrame* aFrame,
                             PRInt32 aNumChars, nsSize& aSize);

  void GetWidgetSize(nsSize& aSize) const { aSize.width  = mWidgetSize.width; 
                                            aSize.height = mWidgetSize.height; }

  // XXX similar functionality needs to be added to widget library and these
  //     need to change to use it.
  static  PRInt32 GetScrollbarWidth(float aPixToTwip);
  virtual PRInt32 GetVerticalBorderWidth(float aPixToTwip) const;
  virtual PRInt32 GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual PRInt32 GetVerticalInsidePadding(float aPixToTwip,
                                           PRInt32 aInnerHeight) const;
  virtual PRInt32 GetHorizontalInsidePadding(float aPixToTwip, 
                                             PRInt32 aInnerWidth) const;

protected:

  virtual ~nsInputFrame();

  /**
    * Return PR_TRUE if the bounds of this frame have been set
    */
  PRBool BoundsAreSet();

  void SetViewVisiblity(nsIPresContext* aPresContext, PRBool aShow);

 /**
    * Get the size that this frame would occupy without any constraints
    * @param aPresContext the presentation context
    * @param aDesiredSize the size desired by this frame, to be set by this method
    * @param aMaxSize the maximum size available for this frame
    */
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

  nsFont& GetFont(nsIPresContext* aPresContext);

   /**
    * Get the width and height of this control based on CSS 
    * @param aPresContext the presentation context
    * @param aSize the size that this frame wants, set by this method. values of -1 
    * for aSize.width or aSize.height indicate unset values.
    */
  void GetStyleSize(nsIPresContext& aContext,
                    const nsReflowState& aReflowState,
                    nsSize& aSize);

  nscoord GetStyleDim(nsIPresContext& aPresContext, nscoord aMaxDim, 
                      nscoord aMaxWidth, const nsStyleCoord& aCoord);

  nsMouseState mLastMouseState;
  nsSize mWidgetSize;
  PRBool mDidInit;
};

#endif

