/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsListControlFrame_h___
#define nsListControlFrame_h___

#include "nsScrollFrame.h"
#include "nsIDOMFocusListener.h"
#include "nsIPresContext.h"
#include "nsIFormControlFrame.h"
#include "nsIListControlFrame.h"


class nsIDOMHTMLSelectElement;
class nsIDOMHTMLCollection;
class nsIDOMHTMLOptionElement;
class nsFormFrame;
class nsScrollFrame;
class nsIComboboxControlFrame;


/**
 * The block frame has two additional named child lists:
 * - "Floater-list" which contains the floated frames
 * - "Bullet-list" which contains the bullet frame
 *
 * @see nsLayoutAtoms::bulletList
 * @see nsLayoutAtoms::floaterList
 */
class nsListControlFrame : public nsScrollFrame, 
                           public nsIFormControlFrame, 
                           public nsIListControlFrame
{
public:
  friend nsresult NS_NewListControlFrame(nsIFrame*& aNewFrame);

   // nsISupports
  NS_DECL_ISUPPORTS

 // nsISupports overrides
 // NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus);

  // nsIFrame
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow);

  NS_IMETHOD Deselect();

      // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

  /*virtual nsresult Focus(nsIDOMEvent* aEvent);
  virtual nsresult Blur(nsIDOMEvent* aEvent);
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD AddEventListener(nsIDOMNode * aNode);
  NS_IMETHOD RemoveEventListener(nsIDOMNode * aNode);

  NS_IMETHOD  DeleteFrame(nsIPresContext& aPresContext);*/

  NS_METHOD GetMultiple(PRBool* aResult, nsIDOMHTMLSelectElement* aSelect = nsnull);

  virtual nsresult GetSizeFromContent(PRInt32* aSize) const;
  NS_IMETHOD GetMaxLength(PRInt32* aSize);

  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);


  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;

  // nsIFrame
  NS_IMETHOD  SetRect(const nsRect& aRect);
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight);

  /////////////////////////
  // nsHTMLContainerFrame
  /////////////////////////
  virtual PRIntn GetSkipSides() const;

  /////////////////////////
  // nsIFormControlFrame
  /////////////////////////
  NS_IMETHOD GetType(PRInt32* aType) const;

  NS_IMETHOD GetName(nsString* aName);

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);
  
  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual void Reset();

  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);

  virtual void SetFormFrame(nsFormFrame* aFrame);

 
  // nsIListControlFrame
  NS_IMETHOD SetComboboxFrame(nsIFrame* aComboboxFrame);
  NS_IMETHOD GetSelectedItem(nsString & aStr);
  NS_IMETHOD AboutToDropDown();
  NS_IMETHOD CaptureMouseEvents(PRBool aGrabMouseEvents);

  // Static Methods
  static nsIDOMHTMLSelectElement* GetSelect(nsIContent * aContent);
  static nsIDOMHTMLCollection*    GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect = nsnull);
  static nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRUint32 aIndex);
  static PRBool                   GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRUint32 aIndex, nsString& aValue);

  nsIContent*                 GetOptionContent(PRUint32 aIndex);
  PRBool IsFrameSelected(PRUint32 aIndex);
  void   SetFrameSelected(PRUint32 aIndex, PRBool aSelected);
 
protected:
  nsListControlFrame();
  virtual ~nsListControlFrame();

  PRInt32 GetNumberOfOptions();

  nsIFrame * GetOptionFromChild(nsIFrame* aParentFrame);

  nsresult GetFrameForPointUsing(const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsIFrame**     aFrame);

  // Utility methods

  void DisplaySelected(nsIContent* aContent); 
  void DisplayDeselected(nsIContent* aContent); 
  void UpdateItem(nsIContent* aContent, PRBool aSelected);

  // nsHTMLContainerFrame overrides
 
  void ClearSelection();
  void InitializeFromContent(PRBool aDoDisplay = PR_FALSE);

  void ExtendedSelection(PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aDoInvert, PRBool aSetValue);

  NS_IMETHOD HandleLikeDropDownListEvent(nsIPresContext& aPresContext, 
                                         nsGUIEvent*     aEvent,
                                         nsEventStatus&  aEventStatus);
  NS_IMETHOD HandleLikeListEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus&  aEventStatus);
  PRInt32 SetContentSelected(nsIFrame *    aHitFrame, 
                             nsIContent *& aHitContent,
                             PRBool        aDisplaySelected);

  // Data Members
  nsFormFrame* mFormFrame;
  PRInt32      mNumRows;
//XXX: TODO: This should not be hardcoded to 64 
//ZZZ  PRBool       mIsFrameSelected[64];
  PRInt32      mNumSelections;
  PRInt32      mMaxNumSelections;
  PRBool       mMultipleSelections;
  PRInt32      mSelectedIndex;
  PRInt32      mStartExtendedIndex;
  PRInt32      mEndExtendedIndex;
  nsIFrame   * mHitFrame;
  nsIContent * mHitContent;
  nsIFrame   * mCurrentHitFrame;
  nsIContent * mCurrentHitContent;
  nsIFrame   * mSelectedFrame;
  nsIContent * mSelectedContent;
  PRBool       mIsInitializedFromContent;
  nsIFrame *   mContentFrame;
  PRBool       mInDropDownMode;
  nsIComboboxControlFrame * mComboboxFrame;
  nsString     mSelectionStr;

};

#endif /* nsListControlFrame_h___ */

