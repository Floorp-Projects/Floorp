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
#include "nsFormControlHelper.h"
#include "nsIFormManager.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"
#include "nsCoord.h"
#include "nsIStyleContext.h"

class nsIView;
class nsIPresContext;
class nsStyleCoord;
class nsFormFrame;

#define CSS_NOTSET -1
#define ATTR_NOTSET -1

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
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

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

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;

   /**
    * Get the width and height of this control based on CSS 
    * @param aPresContext the presentation context
    * @param aSize the size that this frame wants, set by this method. values of -1 
    * for aSize.width or aSize.height indicate unset values.
    */
  static void GetStyleSize(nsIPresContext& aContext,
                            const nsHTMLReflowState& aReflowState,
                            nsSize& aSize);

protected:

  virtual ~nsFormControlFrame();

   // nsIFormControLFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 


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

//
//-------------------------------------------------------------------------------------
//  Utility methods for managing checkboxes and radiobuttons
//-------------------------------------------------------------------------------------
//   
   /**
    * Get the state of the checked attribute.
    * @param aState set to PR_TRUE if the checked attribute is set,
    * PR_FALSE if the checked attribute has been removed
    * @returns NS_OK or NS_CONTENT_ATTR_HAS_VALUE
    */

  nsresult GetCurrentCheckState(PRBool* aState);
 
   /**
    * Set the state of the checked attribute.
    * @param aState set to PR_TRUE to set the attribute,
    * PR_FALSE to unset the attribute
    * @returns NS_OK or NS_CONTENT_ATTR_HAS_VALUE
    */

  nsresult SetCurrentCheckState(PRBool aState);

   /**
    * Get the state of the defaultchecked attribute.
    * @param aState set to PR_TRUE if the defaultchecked attribute is set,
    * PR_FALSE if the defaultchecked attribute has been removed
    * @returns NS_OK or NS_CONTENT_ATTR_HAS_VALUE
    */
 
  nsresult GetDefaultCheckState(PRBool* aState);


  //nscoord GetStyleDim(nsIPresContext& aPresContext, nscoord aMaxDim, 
  //                    nscoord aMaxWidth, const nsStyleCoord& aCoord);

#if 0

//
//-------------------------------------------------------------------------------------
// Utility methods for rendering Form Elements using GFX
//-------------------------------------------------------------------------------------
//
// XXX: The following location for the paint code is TEMPORARY. 
// It is being used to get printing working
// under windows. Later it will be used to GFX-render the controls to the display. 
// Expect this code to repackaged and moved to a new location in the future.

   /**
    * Enumeration of possible mouse states used to detect mouse clicks
    */
   enum nsArrowDirection {
    eArrowDirection_Left,
    eArrowDirection_Right,
    eArrowDirection_Up,
    eArrowDirection_Down
   };

   /**
    * Scale, translate and convert an arrow of poitns from nscoord's to nsPoints's.
    *
    * @param aNumberOfPoints number of (x,y) pairs
    * @param aPoints arrow of points to convert
    * @param aScaleFactor scale factor to apply to each points translation.
    * @param aX x coordinate to add to each point after scaling
    * @param aY y coordinate to add to each point after scaling
    * @param aCenterX x coordinate of the center point in the original array of points.
    * @param aCenterY y coordiante of the center point in the original array of points.
    */

   static void SetupPoints(PRUint32 aNumberOfPoints, nscoord* aPoints, 
     nsPoint* aPolygon, nscoord aScaleFactor, nscoord aX, nscoord aY,
     nscoord aCenterX, nscoord aCenterY);

   /**
    * Paint a fat line. The line is drawn as a polygon with a specified width.
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

  static void PaintLine(nsIRenderingContext& aRenderingContext, 
                 nscoord aSX, nscoord aSY, nscoord aEX, nscoord aEY, 
                 PRBool aHorz, nscoord aWidth, nscoord aOnePixel);

   /**
    * Draw an arrow glyph. 
	  * 
    * @param aRenderingContext the rendering context
    * @param aSX upper left x coordinate pixels
   	* @param aSY upper left y coordinate pixels
    * @param aType @see nsArrowDirection enumeration 
    * @param aOnePixel number of twips in a single pixel.
    */

  static void PaintArrowGlyph(nsIRenderingContext& aRenderingContext, 
                              nscoord aSX, nscoord aSY, nsArrowDirection aArrowDirection, 
                              nscoord aOnePixel);

   /**
    * Draw an arrow 
   	* 
    * @param aArrowDirection @see nsArrowDirection enumeration
    * @param aRenderingContext the rendering context
		* @param aPresContext the presentation context
		* @param aDirtyRect rectangle requiring update
    * @param aOnePixel number of TWIPS in a single pixel
		* @param aColor color of the arrow glph
		* @param aSpacing spacing for the arrow background
		* @param aForFrame frame which the arrow will be rendered into.
    * @param aFrameRect rectangle for the frame specified by aForFrame
    */

  static void PaintArrow(nsArrowDirection aArrowDirection,
					    nsIRenderingContext& aRenderingContext,
						nsIPresContext& aPresContext, 
						const nsRect& aDirtyRect,
                        nsRect& aRect, 
						nscoord aOnePixel, 
						nsIStyleContext* aArrowStyle,
						const nsStyleSpacing& aSpacing,
						nsIFrame* aForFrame,
                        nsRect& aFrameRect);
   /**
    * Paint a scrollbar
	  * 
    * @param aRenderingContext the rendering context
		* @param aPresContext the presentation context
		* @param aDirtyRect rectangle requiring update
    * @param aRect width and height of the scrollbar
		* @param aHorizontal if TRUE scrollbar is drawn horizontally, vertical if FALSE
		* @param aOnePixel number TWIPS per pixel
    * @param aScrollbarStyleContext style context for the scrollbar
    * @param aScrollbarArrowStyleContext style context for the scrollbar arrow
		* @param aForFrame the frame that the scrollbar will be rendered in to
    * @param aFrameRect the rectangle for the frame passed as aForFrame
    */

  static void PaintScrollbar(nsIRenderingContext& aRenderingContext,
																	nsIPresContext& aPresContext, 
																  const nsRect& aDirtyRect,
                                  nsRect& aRect, 
																  PRBool aHorizontal, 
																  nscoord aOnePixel, 
                                  nsIStyleContext* aScrollbarStyleContext,
                                  nsIStyleContext* aScrollbarArrowStyleContext,
																	nsIFrame* aForFrame,
                                  nsRect& aFrameRect);
   /**
    * Paint a fixed size checkmark
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    */

  static void PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext, 
                                     float aPixelsToTwips);

   /**
    * Paint a fixed size checkmark border
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    * @param aBackgroundColor color for background of checkbox 
    */

  static void PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, const nsStyleColor& aBackgroundColor);

   /**
    * Paint a rectangular button. Includes background, string, and focus indicator
	  * 
    * @param aPresContext the presentation context
    * @param aRenderingContext the rendering context
    * @param aDirtyRect rectangle requiring update
    * @param aWidth width the checkmark border in TWIPS
    * @param aHeight height of the checkmark border in TWIPS
    * @param aShift if PR_TRUE offset button as if it were pressed
    * @param aShowFocus if PR_TRUE draw focus rectangle over button
    * @param aStyleContext style context used for drawing button background 
    * @param aLabel label for button
    * @param aForFrame the frame that the scrollbar will be rendered in to
    */

  static void PaintRectangularButton(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, PRUint32 aWidth, 
                            PRUint32 aHeight, PRBool aShift, PRBool aShowFocus,
                            nsIStyleContext* aStyleContext, nsString& aLabel, 
                            nsFormControlFrame* aForFrame);
   /**
    * Paint a focus indicator.
	  * 
    * @param aRenderingContext the rendering context
    * @param aDirtyRect rectangle requiring update
	  * @param aInside border inside
    * @param aOutside border outside
    */

  static void PaintFocus(nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect, nsRect& aInside, nsRect& aOutside);

   /**
    * Paint a circular border
	  * 
    * @param aPresContext the presentation context
    * @param aRenderingContext the rendering context
    * @param aDirtyRect rectangle requiring update
    * @param aStyleContext style context specifying colors and spacing
    * @param aInset if PR_TRUE draw inset, otherwise draw outset
    * @param aForFrame the frame that the scrollbar will be rendered in to
    * @param aWidth width of the border in TWIPS
    * @param aHeight height ofthe border in TWIPS
    */

  static void PaintCircularBorder(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect, nsIStyleContext* aStyleContext, PRBool aInset,
                         nsIFrame* aForFrame, PRUint32 aWidth, PRUint32 aHeight);

#endif
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

