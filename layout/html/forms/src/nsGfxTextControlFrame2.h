/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsFileControlFrame_h___
#define nsFileControlFrame_h___

#include "nsAreaFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIStatefulFrame.h"
#include "nsIEditor.h"
#include "nsIGfxTextControlFrame.h"
#include "nsFormControlHelper.h"//for the inputdimensions
#include "nsHTMLValue.h" //for nsHTMLValue
#include "nsIFontMetrics.h"

class nsIPresState;
class nsGfxTextControlFrame;
class nsFormFrame;
class nsISupportsArray;
class nsIHTMLContent;
class nsIEditor;
class nsISelectionController;
class nsTextAreaSelectionImpl;




class nsGfxTextControlFrame2 : public nsHTMLContainerFrame,
                               public nsIAnonymousContentCreator,
                               public nsIFormControlFrame,
                               public nsIGfxTextControlFrame2,
                               public nsIStatefulFrame

{
public:
  nsGfxTextControlFrame2();
  virtual ~nsGfxTextControlFrame2();

  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
    aResult.AssignWithConversion("nsGfxControlFrame2");
    return NS_OK;
  }
#endif

  // from nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                    nsISupportsArray& aChildList);

  // Utility methods to get and set current widget state
  void GetTextControlFrameState(nsString& aValue);
  void SetTextControlFrameState(const nsString& aValue);
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);

//==== BEGIN NSIFORMCONTROLFRAME
  NS_IMETHOD GetType(PRInt32* aType) const; //*
  NS_IMETHOD GetName(nsString* aName);//*
  virtual void SetFocus(PRBool aOn , PRBool aRepaint); 
  virtual void ScrollIntoView(nsIPresContext* aPresContext);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset(nsIPresContext* aPresContext);
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual PRInt32 GetMaxNumValues();/**/
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);
  virtual void SetFormFrame(nsFormFrame* aFrame);
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;/**/
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);
  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                     const nsFont*&  aFont);
  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 


//==== END NSIFORMCONTROLFRAME

//==== NSIGFXTEXTCONTROLFRAME2

  NS_IMETHOD    GetEditor(nsIEditor **aEditor);
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength);
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart);
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd);
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd);
  NS_IMETHOD    GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);
  NS_IMETHOD    GetSelectionController(nsISelectionController **aSelCon);

//==== END NSIGFXTEXTCONTROLFRAME2
//==== OVERLOAD of nsIFrame
  /** handler for attribute changes to mContent */
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  NS_IMETHOD GetText(nsString* aText, PRBool aInitialValue);

  NS_DECL_ISUPPORTS_INHERITED
protected:
  nsString *GetCachedString();
  virtual PRIntn GetSkipSides() const;
  void RemoveNewlines(nsString &aString);
  NS_IMETHOD GetMaxLength(PRInt32* aSize);
  NS_IMETHOD DoesAttributeExist(nsIAtom *aAtt);

//helper methods
  virtual PRBool IsSingleLineTextControl() const;
  virtual PRBool IsPlainTextControl() const;
  virtual PRBool IsPasswordTextControl() const;
  nsresult GetSizeFromContent(PRInt32* aSize) const;
  PRInt32 GetDefaultColumnWidth() const { return (PRInt32)(20); } // this was DEFAULT_PIXEL_WIDTH

  nsresult GetColRowSizeAttr(nsIFormControlFrame*  aFrame,
                             nsIAtom *     aColSizeAttr,
                             nsHTMLValue & aColSize,
                             nsresult &    aColStatus,
                             nsIAtom *     aRowSizeAttr,
                             nsHTMLValue & aRowSize,
                             nsresult &    aRowStatus);

  NS_IMETHOD ReflowStandard(nsIPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus,
                            nsMargin&                aBorder,
                            nsMargin&                aPadding);

  PRInt32 CalculateSizeStandard (nsIPresContext*       aPresContext, 
                                  nsIRenderingContext*  aRendContext,
                                  nsIFormControlFrame*  aFrame,
                                  const nsSize&         aCSSSize, 
                                  nsInputDimensionSpec& aSpec, 
                                  nsSize&               aDesiredSize, 
                                  nsSize&               aMinSize, 
                                  PRBool&               aWidthExplicit, 
                                  PRBool&               aHeightExplicit, 
                                  nscoord&              aRowHeight,
                                  nsMargin&             aBorder,
                                  nsMargin&             aPadding);

  PRInt32 CalculateSizeNavQuirks (nsIPresContext*       aPresContext, 
                                  nsIRenderingContext*  aRendContext,
                                  nsIFormControlFrame*  aFrame,
                                  const nsSize&         aCSSSize, 
                                  nsInputDimensionSpec& aSpec, 
                                  nsSize&               aDesiredSize, 
                                  nsSize&               aMinSize, 
                                  PRBool&               aWidthExplicit, 
                                  PRBool&               aHeightExplicit, 
                                  nscoord&              aRowHeight,
                                  nsMargin&             aBorder,
                                  nsMargin&             aPadding);


  NS_IMETHOD ReflowNavQuirks(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus,
                              nsMargin&                aBorder,
                              nsMargin&                aPadding);

  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                               nsIContent *      aContent,
                               nsIFrame**        aFrame);

  PRInt32 GetWidthInCharacters() const;

//nsIStatefulFrame
  NS_IMETHOD GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);


private:
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsISelectionController> mSelCon;

  //cached sizes and states
  nsString*    mCachedState;
  nscoord      mSuggestedWidth;
  nscoord      mSuggestedHeight;

  PRBool mIsProcessing;
  nsFormFrame *mFormFrame;
  nsTextAreaSelectionImpl *mTextSelImpl;
};

#endif


