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

#ifndef nsFormControlFrame_h___
#define nsFormControlFrame_h___

#include "nsIFormControlFrame.h"
#include "nsIFormManager.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"
#include "nsCoord.h"

class nsIView;
class nsIPresContext;
class nsStyleCoord;
class nsFormFrame;

#define CSS_NOTSET -1
#define ATTR_NOTSET -1

/**
  * Enumeration of possible mouse states used to detect mouse clicks
  */
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseExit,
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
  * nsFormControlFrame is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsFormControlFrame : public nsLeafFrame,
                           public nsIFormControlFrame
{

public:
  /**
    * Main constructor
    * @param aContent the content representing this frame
    * @param aParentFrame the parent frame
    */
  nsFormControlFrame();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  static nscoord CalculateSize (nsIPresContext* aPresContext, nsFormControlFrame* aFrame,
                                const nsSize& aCSSSize, nsInputDimensionSpec& aDimensionSpec, 
                                nsSize& aBounds, PRBool& aWidthExplicit, 
                                PRBool& aHeightExplicit, nscoord& aRowSize,
                                nsIRenderingContext *aRendContext);

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

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext&      aCX,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  /**
    * Respond to the request to print the control
    *
    */
  NS_IMETHOD ReflowWithNoWidget(nsIPresContext&      aPresContext,
                                       nsHTMLReflowMetrics& aDesiredSize,
                                       const nsHTMLReflowState& aReflowState,
                                       nsReflowStatus&      aStatus);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);
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

  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD GetValue(nsString* aName);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);

  /**
    * Get the widget associated with this frame
    * @param aView the view associated with the frame. It is a convience parm.
    * @param aWidget the address of address of where the widget will be placed.
    * This method doses an AddRef on the widget.
    */
  nsresult GetWidget(nsIView* aView, nsIWidget** aWidget);
  nsresult GetWidget(nsIWidget** aWidget);

  /**
    * Respond to a enter key being pressed
    */
  virtual void EnterPressed(nsIPresContext& aPresContext) {} 

  /**
    * Respond to a mouse click (e.g. mouse enter, mouse down, mouse up)
    */
  virtual void MouseClicked(nsIPresContext* aPresContext) {}

  /**
    * Perform opertations after the widget associated with this frame has been
    * created.
    */
  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth, nscoord& aHeight);

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);

  void SetColors(nsIPresContext& aPresContext);
  
  virtual void Reset();
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);

  /**
    * Perform opertations before the widget associated with this frame has been
    * created.
    */
  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);  

  static nscoord GetTextSize(nsIPresContext& aContext, nsFormControlFrame* aFrame,
                             const nsString& aString, nsSize& aSize,
                             nsIRenderingContext *aRendContext);
  static nscoord GetTextSize(nsIPresContext& aContext, nsFormControlFrame* aFrame,
                             PRInt32 aNumChars, nsSize& aSize,
                             nsIRenderingContext *aRendContext);

  void GetWidgetSize(nsSize& aSize) const { aSize.width  = mWidgetSize.width; 
                                            aSize.height = mWidgetSize.height; }

  // XXX similar functionality needs to be added to widget library and these
  //     need to change to use it.
  static  nscoord GetScrollbarWidth(float aPixToTwip);
  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  NS_IMETHOD GetSize(PRInt32* aSize) const;
  NS_IMETHOD GetMaxLength(PRInt32* aSize);

  virtual void SetClickPoint(nscoord aX, nscoord aY);
  nsFormFrame* GetFormFrame() { return mFormFrame; }
  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }

protected:

  virtual ~nsFormControlFrame();

  /**
    * Get the size that this frame would occupy without any constraints
    * @param aPresContext the presentation context
    * @param aDesiredSize the size desired by this frame, to be set by this method
    * @param aMaxSize the maximum size available for this frame
    */
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, nsFont& aFont);

   /**
    * Get the width and height of this control based on CSS 
    * @param aPresContext the presentation context
    * @param aSize the size that this frame wants, set by this method. values of -1 
    * for aSize.width or aSize.height indicate unset values.
    */
  void GetStyleSize(nsIPresContext& aContext,
                    const nsHTMLReflowState& aReflowState,
                    nsSize& aSize);

  //nscoord GetStyleDim(nsIPresContext& aPresContext, nscoord aMaxDim, 
  //                    nscoord aMaxWidth, const nsStyleCoord& aCoord);

   /**
    * Draw a fat line. The line is drawn as a polygon with a specified width.
	* Utility used for rendering a form control during printing.
	* 
    * @param aRenderingContext the rendering context
    * @param aSX starting x in pixels
	* @param aSY starting y in pixels
	* @param aEX ending x in pixels
	* @param aEY ending y in pixels
    * @param aHorz PR_TRUE if aWidth is added to x coordinates to form polygon. If 
	*              PR_FALSE  then aWidth as added to the y coordinates.
    * @param aOnePixel number of twips in a single pixel.
    */

  void DrawLine(nsIRenderingContext& aRenderingContext, 
                 nscoord aSX, nscoord aSY, nscoord aEX, nscoord aEY, 
                 PRBool aHorz, nscoord aWidth, nscoord aOnePixel);


  nsMouseState mLastMouseState;
  nsIWidget*   mWidget;
  nsSize       mWidgetSize;
  PRBool       mDidInit;
  nsPoint      mLastClickPoint;
  nsFormFrame* mFormFrame;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

};

#endif

