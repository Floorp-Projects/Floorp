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

#define NS_FORMSIZE_NOTSET -1

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
    * Respond to a enter key being pressed
    */
  virtual void EnterPressed(nsIPresContext& aPresContext) {} 

  /**
    * Respond to a mouse click (e.g. mouse enter, mouse down, mouse up)
    */
  virtual void MouseClicked(nsIPresContext* aPresContext) {}

  /**
    * Respond to a control change (e.g. combo box close-up)
    */
  virtual void ControlChanged(nsIPresContext* aPresContext) {}

  /**
    * Perform opertations after the widget associated with this frame has been
    * created.
    */
  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth, nscoord& aHeight);

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);
  
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
  virtual nsresult GetSizeFromContent(PRInt32* aSize) const;
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

    // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

protected:

  virtual ~nsFormControlFrame();

 
  /**
   * Determine if the control uses a native widget for rendering
   * @param aRequiresWidget is set to PR_TRUE if it has a native widget, PR_FALSE otherwise.
   * @returns NS_OK 
   */

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);
 

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

  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);

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
    * PR_FALSE if the checked attribute has been removed
    * @returns NS_OK or NS_CONTENT_ATTR_HAS_VALUE
    */
 
  nsresult GetDefaultCheckState(PRBool* aState);

   /**
    * Set the state of the checked attribute.
    * @param aState set to PR_TRUE to set the checked attribute 
    * PR_FALSE to unset it
    * @returns NS_OK 
    */

  nsresult SetDefaultCheckState(PRBool aState);

  nsSize       mWidgetSize;
  PRBool       mDidInit;
  nsPoint      mLastClickPoint;
  nsFormFrame* mFormFrame;
  nscoord      mSuggestedWidth;
  nscoord      mSuggestedHeight;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

};

#endif

