/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFormControlHelper_h___
#define nsFormControlHelper_h___

#include "nsIFormControlFrame.h"
#include "nsIFormManager.h"
#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsLeafFrame.h"
#include "nsCoord.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"

class nsIView;
//class nsIPresContext;
class nsStyleCoord;
class nsFormFrame;
//class nsIStyleContext;

#define CSS_NOTSET -1
#define ATTR_NOTSET -1

#define NS_STRING_TRUE   NS_LITERAL_STRING("1")
#define NS_STRING_FALSE  NS_LITERAL_STRING("0")

// for localization
#define FORM_PROPERTIES "chrome://communicator/locale/layout/HtmlForm.properties"


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
  * nsFormControlHelper is the base class for frames of form controls. It
  * provides a uniform way of creating widgets, resizing, and painting.
  * @see nsLeafFrame and its base classes for more info
  */
class nsFormControlHelper 
{

public:
  

  static nscoord CalculateSize (nsIPresContext*       aPresContext, 
                                nsIRenderingContext*  aRendContext,
                                nsIFormControlFrame*  aFrame,
                                const nsSize&         aCSSSize, 
                                nsInputDimensionSpec& aDimensionSpec, 
                                nsSize&               aDesiredSize,
                                nsSize&               aMinSize, 
                                PRBool&               aWidthExplicit, 
                                PRBool&               aHeightExplicit, 
                                nscoord&              aRowSize);

  static nscoord GetTextSize(nsIPresContext* aContext, nsIFormControlFrame* aFrame,
                             const nsString& aString, nsSize& aSize,
                             nsIRenderingContext *aRendContext);

  static nscoord GetTextSize(nsIPresContext* aContext, nsIFormControlFrame* aFrame,
                             PRInt32 aNumChars, nsSize& aSize,
                             nsIRenderingContext *aRendContext);

  static nsresult GetFont(nsIFormControlFrame *  aFormFrame,
                          nsIPresContext*        aPresContext, 
                          nsIStyleContext *      aStyleContext, 
                          const nsFont*&         aFont);

  // returns the an addref'ed FontMetrics for the default font for the frame
  static nsresult GetFrameFontFM(nsIPresContext* aPresContext, 
                                 nsIFormControlFrame * aFrame,
                                 nsIFontMetrics** aFontMet);

  static nscoord CalcNavQuirkSizing(nsIPresContext*      aPresContext, 
                                    nsIRenderingContext* aRendContext,
                                    nsIFontMetrics*      aFontMet, 
                                    nsIFormControlFrame* aFrame,
                                    nsInputDimensionSpec& aSpec,
                                    nsSize&              aSize);

  static void ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame);

  // Map platform line endings (CR, CRLF, LF) to DOM line endings (LF)
  static void PlatformToDOMLineBreaks(nsString &aString);

  static nsresult GetValue(nsIContent* aContent, nsString* aResult);
  static nsresult GetName(nsIContent* aContent, nsString* aResult);
  static nsresult GetInputElementValue(nsIContent* aContent, nsString* aText, PRBool aInitialValue);

 /** 
  * Utility to convert a string to a PRBool
  * @param aValue string to convert to a PRBool
  * @returns PR_TRUE if aValue = "1", PR_FALSE otherwise
  */

  static PRBool GetBool(const nsAReadableString& aValue);

 /** 
  * Utility to convert a PRBool to a string
  * @param aValue Boolean value to convert to string.
  * @param aResult string to hold the boolean value. It is set to "1" 
  *        if aValue equals PR_TRUE, "0" if aValue equals PR_FALSE.

  */

  static void  GetBoolString(const PRBool aValue, nsAWritableString& aResult);

  static nsCompatibility GetRepChars(nsIPresContext* aPresContext, char& char1, char& char2);

  // wrap can be one of these three values.  
  typedef enum {
    eHTMLTextWrap_Off     = 1,    // the default
    eHTMLTextWrap_Hard    = 2,    // "hard" or "physical"
    eHTMLTextWrap_Soft    = 3     // "soft" or "virtual"
  } nsHTMLTextWrap;

  /** returns the value of the "wrap" property in aOutValue
    * returns NS_CONTENT_ATTR_NOT_THERE if the property does not exist for this
    */
  static nsresult GetWrapProperty(nsIContent * aContent, nsString &aOutValue);

  static nsresult GetWrapPropertyEnum(nsIContent * aContent, nsHTMLTextWrap& aWrapProp);

  // Localization Helper
  static nsresult GetLocalizedString(const char * aPropFileName, const PRUnichar* aKey, nsString& oVal);
  static const char * GetHTMLPropertiesFileName() { return FORM_PROPERTIES; }

  static nsresult GetDisabled(nsIContent* aContent, PRBool* oIsDisabled);

  // If the PresShell is null then the PresContext will get its own and use it
  static nsresult DoManualSubmitOrReset(nsIPresContext* aPresContext,
                                        nsIPresShell*   aPresShell,
                                        nsIFrame*       aFormFrame,
                                        nsIFrame*       aFormControlFrame,
                                        PRBool          aDoSubmit,     // Submit = TRUE, Reset = FALSE
                                        PRBool          aDoDOMEvent);

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

  //nsresult GetCurrentCheckState(PRBool* aState);
 
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
    * Paint a fixed size checkmark
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    */

  static void PaintFixedSizeCheckMark(nsIRenderingContext& aRenderingContext, 
                                     float aPixelsToTwips);
                       
  /**
    * Paint a checkmark
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    * @param aWidth width in twips
    * @param aHeight height in twips
    */

  static void PaintCheckMark(nsIRenderingContext& aRenderingContext,
                             float aPixelsToTwips, const nsRect & aRect);

   /**
    * Paint a fixed size checkmark border
	  * 
    * @param aRenderingContext the rendering context
	  * @param aPixelsToTwips scale factor for convering pixels to twips.
    * @param aBackgroundColor color for background of the checkbox 
    */

  static void PaintFixedSizeCheckMarkBorder(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, const nsStyleColor& aBackgroundColor);

   /**
    * Paint a rectangular button. Includes background, string, and focus indicator
	  * 
    * @param aPresContext the presentation context
    * @param aRenderingContext the rendering context
    * @param aDirtyRect rectangle requiring update
    * @param aRect  x,y, width, and height of the button in TWIPS
    * @param aShift if PR_TRUE offset button as if it were pressed
    * @param aShowFocus if PR_TRUE draw focus rectangle over button
    * @param aStyleContext style context used for drawing button background 
    * @param aLabel label for button
    * @param aForFrame the frame that the scrollbar will be rendered in to
    */

  static void PaintRectangularButton(nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect, const nsRect& aRect, 
                            PRBool aShift, PRBool aShowFocus, PRBool aDisabled,
							              PRBool aDrawOutline,
							              nsIStyleContext* aOutlineStyle,
							              nsIStyleContext* aFocusStyle,
                            nsIStyleContext* aStyleContext, nsString& aLabel, 
                            nsIFrame* aForFrame);

  static void GetFormCompatibilityMode(nsIPresContext* aPresContext, nsCompatibility& mCompatMode);

protected:
  nsFormControlHelper();
  virtual ~nsFormControlHelper();

  // Temporary - Test of full time Standard mode for forms (Bug 91602)
  static PRPackedBool mCompatFirstTime;
  static PRPackedBool mUseEitherMode;
};

#endif

