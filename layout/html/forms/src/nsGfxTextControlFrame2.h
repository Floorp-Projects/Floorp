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

#ifndef nsGfxTextControlFrame2_h___
#define nsGfxTextControlFrame2_h___

#include "nsStackFrame.h"
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
#include "nsWeakReference.h" //for service and presshell pointers
#include "nsIScrollableViewProvider.h"

class nsIPresState;
class nsFormFrame;
class nsISupportsArray;
class nsIHTMLContent;
class nsIEditor;
class nsISelectionController;
class nsTextInputSelectionImpl;
class nsTextInputListener;
class nsIDOMCharacterData;
class nsIScrollableView;
#ifdef ACCESSIBILITY
class nsIAccessible;
#endif


class nsGfxTextControlFrame2 : public nsStackFrame,
                               public nsIAnonymousContentCreator,
                               public nsIFormControlFrame,
                               public nsIGfxTextControlFrame2,
                               public nsIStatefulFrame,
                               public nsIScrollableViewProvider

{
public:
  nsGfxTextControlFrame2(nsIPresShell* aShell);
  virtual ~nsGfxTextControlFrame2();

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);//remove yourself as a form control

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags = 0);

  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

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
  void GetTextControlFrameState(nsAWritableString& aValue);
  void SetTextControlFrameState(const nsAReadableString& aValue);
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
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 


//==== END NSIFORMCONTROLFRAME

//==== NSIGFXTEXTCONTROLFRAME2

  NS_IMETHOD    GetEditor(nsIEditor **aEditor);
  NS_IMETHOD    GetTextLength(PRInt32* aTextLength);
  NS_IMETHOD    SetSelectionStart(PRInt32 aSelectionStart);
  NS_IMETHOD    SetSelectionEnd(PRInt32 aSelectionEnd);
  NS_IMETHOD    SetSelectionRange(PRInt32 aSelectionStart, PRInt32 aSelectionEnd);
  NS_IMETHOD    GetSelectionRange(PRInt32* aSelectionStart, PRInt32* aSelectionEnd);
  NS_IMETHOD    GetSelectionContr(nsISelectionController **aSelCon);

//==== END NSIGFXTEXTCONTROLFRAME2
//==== OVERLOAD of nsIFrame
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  /** handler for attribute changes to mContent */
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType, 
                              PRInt32         aHint);

  NS_IMETHOD GetText(nsString* aText, PRBool aInitialValue);

  NS_DECL_ISUPPORTS_INHERITED

public: //for methods who access nsGfxTextControlFrame2 directly

  void SubmitAttempt();
  NS_IMETHOD InternalContentChanged();//notify that we have some kind of change.
  NS_IMETHOD CallOnChange();
  virtual PRBool IsSingleLineTextControl() const;
  virtual PRBool IsPlainTextControl() const;
  virtual PRBool IsPasswordTextControl() const;

protected:

  PRBool IsScrollable() const;
  nsresult SetInitialValue();
  void InvalidateCachedState();
  virtual PRIntn GetSkipSides() const;
  void RemoveNewlines(nsString &aString);
  NS_IMETHOD GetMaxLength(PRInt32* aSize);
  NS_IMETHOD DoesAttributeExist(nsIAtom *aAtt);

//helper methods
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
                            nsSize&                  aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus,
                            nsMargin&                aBorder,
                            nsMargin&                aPadding);

  PRInt32 CalculateSizeStandard (nsIPresContext*       aPresContext, 
                                  nsIRenderingContext*  aRendContext,
                                  nsIFormControlFrame*  aFrame,
                                  nsInputDimensionSpec& aSpec, 
                                  nsSize&               aDesiredSize, 
                                  nsSize&               aMinSize, 
                                  nscoord&              aRowHeight,
                                  nsMargin&             aBorder,
                                  nsMargin&             aPadding,
                                  PRBool                aIsUsingDefSize);

  PRInt32 CalculateSizeNavQuirks (nsIPresContext*       aPresContext, 
                                  nsIRenderingContext*  aRendContext,
                                  nsIFormControlFrame*  aFrame,
                                  nsInputDimensionSpec& aSpec, 
                                  nsSize&               aDesiredSize, 
                                  nsSize&               aMinSize, 
                                  nscoord&              aRowHeight,
                                  nsMargin&             aBorder,
                                  nsMargin&             aPadding,
                                  PRBool                aIsUsingDefSize);


  NS_IMETHOD ReflowNavQuirks(nsIPresContext*          aPresContext,
                              nsSize&                 aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus,
                              nsMargin&                aBorder,
                              nsMargin&                aPadding);

  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                               nsIContent *      aContent,
                               nsIFrame**        aFrame);

  PRInt32 GetWidthInCharacters() const;

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

  // nsIScrollableViewProvider
  NS_IMETHOD GetScrollableView(nsIScrollableView** aView);

private:
  //helper methods
  enum {
    eIgnoreSelect = -2,
    eSelectToEnd  = -1  
  };

  NS_IMETHODIMP GetFirstTextNode(nsIDOMCharacterData* *aFirstTextNode);
  nsresult SelectAllContents();
  nsresult SetSelectionEndPoints(PRInt32 aSelStart, PRInt32 aSelEnd);
  
private:
  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsISelectionController> mSelCon;

  //cached sizes and states
  nsString*    mCachedState;
  nscoord      mSuggestedWidth;
  nscoord      mSuggestedHeight;
  nsSize       mSize;

  PRPackedBool mUseEditor;
  PRPackedBool mIsProcessing;
  PRPackedBool mNotifyOnInput;//default this to off to stop any notifications until setup is complete

  nsFormFrame *mFormFrame;
  nsTextInputSelectionImpl *mTextSelImpl;
  nsTextInputListener *mTextListener;
  nsIScrollableView *mScrollableView;
};

#endif


