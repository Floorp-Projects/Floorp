/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFormControlFrame_h___
#define nsFormControlFrame_h___

#include "nsIFormControlFrame.h"
#include "nsFormControlHelper.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"
#include "nsCoord.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"

class nsIView;
class nsPresContext;
class nsStyleCoord;

#define CSS_NOTSET -1
#define ATTR_NOTSET -1

#define NS_FORMSIZE_NOTSET -1

#ifdef DEBUG_rods

#define COMPARE_QUIRK_SIZE(__class, __navWidth, __navHeight) \
{ \
  float t2p;                                            \
  t2p = aPresContext->TwipsToPixels();                  \
  printf ("%-25s::Size=%4d,%4d %3d,%3d Nav:%3d,%3d Diffs: %3d,%3d\n",  \
           (__class),                                   \
           aDesiredSize.width, aDesiredSize.height,     \
           NSToCoordRound(aDesiredSize.width * t2p),    \
           NSToCoordRound(aDesiredSize.height * t2p),   \
           (__navWidth),                                \
           (__navHeight),                               \
           NSToCoordRound(aDesiredSize.width * t2p) - (__navWidth),   \
           NSToCoordRound(aDesiredSize.height * t2p) - (__navHeight)); \
}

#else
#define COMPARE_QUIRK_SIZE(__class, __navWidth, __navHeight)
#endif

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
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

   /**
    * Draw this frame within the context of a presentation context and rendering context
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame** aFrame);

  NS_IMETHOD SetInitialChildList(nsPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD DidReflow(nsPresContext*           aPresContext,
                       const nsHTMLReflowState*  aReflowState,
                       nsDidReflowStatus         aStatus);

  /**
    * Respond to the request to resize and/or reflow
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsPresContext*      aCX,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD Destroy(nsPresContext *aPresContext);

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

  NS_IMETHOD_(PRInt32) GetFormControlType() const;
  NS_IMETHOD GetName(nsAString* aName);
  NS_IMETHOD GetValue(nsAString* aName);

  /**
    * Respond to a enter key being pressed
    */
  virtual void EnterPressed(nsPresContext* aPresContext) {} 

  /**
    * Respond to a mouse click (e.g. mouse enter, mouse down, mouse up)
    */
  virtual void MouseClicked(nsPresContext* aPresContext) {}

  /**
    * Respond to a control change (e.g. combo box close-up)
    */
  virtual void ControlChanged(nsPresContext* aPresContext) {}

  /**
    * Chance to Initialize to a defualt value
    */
  virtual void InitializeControl(nsPresContext* aPresContext);

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);
  virtual void ScrollIntoView(nsPresContext* aPresContext);

  /**
    * Perform opertations before the widget associated with this frame has been
    * created.
    */
  virtual nsWidgetInitData* GetWidgetInitData(nsPresContext* aPresContext);  

  void GetWidgetSize(nsSize& aSize) const { aSize.width  = mWidgetSize.width; 
                                            aSize.height = mWidgetSize.height; }

  // XXX similar functionality needs to be added to widget library and these
  //     need to change to use it.
  static  nscoord GetScrollbarWidth(float aPixToTwip);

  virtual nsresult GetSizeFromContent(PRInt32* aSize) const;
  NS_IMETHOD GetMaxLength(PRInt32* aSize);

  virtual void SetClickPoint(nscoord aX, nscoord aY);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;

   /**
    * Get the width and height of this control based on CSS 
    * @param aPresContext the presentation context
    * @param aSize the size that this frame wants, set by this method. values of -1 
    * for aSize.width or aSize.height indicate unset values.
    */
  static void GetStyleSize(nsPresContext* aContext,
                            const nsHTMLReflowState& aReflowState,
                            nsSize& aSize);

    // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue);

  NS_IMETHOD GetProperty(nsIAtom* aName, nsAString& aValue); 
  // Resize Reflow Optimiaztion Methods
  static void SetupCachedSizes(nsSize& aCacheSize,
                               nscoord& aCachedAscent,
                               nscoord& aCachedMaxElementWidth,
                               nsHTMLReflowMetrics& aDesiredSize);

  static void SkipResizeReflow(nsSize& aCacheSize,
                               nscoord& aCachedAscent,
                               nscoord& aCachedMaxElementWidth,
                               nsSize& aCachedAvailableSize,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus,
                               PRBool& aBailOnWidth,
                               PRBool& aBailOnHeight);
  // AccessKey Helper function
  static nsresult RegUnRegAccessKey(nsPresContext* aPresContext, nsIFrame * aFrame, PRBool aDoReg);

  /**
   * Helper routine to that returns the height of the screen
   *
   */
  static nsresult GetScreenHeight(nsPresContext* aPresContext, nscoord& aHeight);

  /**
   * Helper method to get the absolute position of a frame
   *
   */
  static nsresult GetAbsoluteFramePosition(nsPresContext* aPresContext,
                                           nsIFrame *aFrame, 
                                           nsRect& aAbsoluteTwipsRect, 
                                           nsRect& aAbsolutePixelRect);
protected:

  virtual ~nsFormControlFrame();

  /** 
    * Get the size that this frame would occupy without any constraints
    * @param aPresContext the presentation context
    * @param aDesiredSize the size desired by this frame, to be set by this method
    * @param aMaxSize the maximum size available for this frame
    */
  virtual void GetDesiredSize(nsPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  virtual void GetDesiredSize(nsPresContext* aPresContext,
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
  nscoord      mSuggestedWidth;
  nscoord      mSuggestedHeight;

  nsPresContext * mPresContext;

  // Reflow Optimization
  nsSize       mCacheSize;
  nscoord      mCachedAscent;
  nscoord      mCachedMaxElementWidth;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

};

#endif

