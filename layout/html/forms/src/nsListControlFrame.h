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

//#include "nsHTMLContainerFrame.h"
#include "nsScrollFrame.h"
#include "nsIDOMFocusListener.h"
#include "nsIPresContext.h"
#include "nsIFormControlFrame.h"
#ifdef PLUGGABLE_EVENTS
#include "nsIPluggableEventListener.h"
#endif
#include "nsIListControlFrame.h"

class nsIDOMHTMLSelectElement;
class nsIDOMHTMLCollection;
class nsIDOMHTMLOptionElement;
class nsFormFrame;
class nsScrollFrame;
class nsIComboboxControlFrame;

#if 0
struct nsInputDimSpec
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

  nsInputDimSpec(nsIAtom* aColSizeAttr, PRBool aColSizeAttrInPixels, 
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
#endif
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
#ifdef PLUGGABLE_EVENTS
                            public nsIPluggableEventListener
#endif
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

  //
/*  virtual nsresult AppendNewFrames(nsIPresContext& aPresContext, nsIFrame*);

  virtual nsresult InsertNewFrame(nsIPresContext&  aPresContext,
                          nsBaseIBFrame* aParentFrame,
                          nsIFrame* aNewFrame,
                          nsIFrame* aPrevSibling);

  virtual nsresult DoRemoveFrame(nsBlockReflowState& aState,
                         nsBaseIBFrame* aParentFrame,
                         nsIFrame* aDeletedFrame,
                         nsIFrame* aPrevSibling);
                         */

  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext);

  NS_IMETHOD Deselect();

      // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 


#if 0
  virtual void GetStyleSize(nsIPresContext& aContext,
                    const nsHTMLReflowState& aReflowState,
                    nsSize& aSize);
#endif

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
#if 0
  nscoord CalculateSize (nsIPresContext* aPresContext, nsListControlFrame* aFrame,
                                const nsSize& aCSSSize, nsInputDimSpec& aDimensionSpec, 
                                nsSize& aBounds, PRBool& aWidthExplicit, 
                                PRBool& aHeightExplicit, nscoord& aRowSize,
                                nsIRenderingContext *aRendContext);
#endif

  /*virtual nsresult Focus(nsIDOMEvent* aEvent);
  virtual nsresult Blur(nsIDOMEvent* aEvent);
  virtual nsresult ProcessEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD AddEventListener(nsIDOMNode * aNode);
  NS_IMETHOD RemoveEventListener(nsIDOMNode * aNode);

  NS_IMETHOD  DeleteFrame(nsIPresContext& aPresContext);*/

  NS_METHOD GetMultiple(PRBool* aResult, nsIDOMHTMLSelectElement* aSelect = nsnull);

  ///XXX: End o the temporary methods
#if 0
  nscoord GetTextSize(nsIPresContext& aContext, nsListControlFrame* aFrame,
                             const nsString& aString, nsSize& aSize,
                             nsIRenderingContext *aRendContext);

  nscoord GetTextSize(nsIPresContext& aContext, nsListControlFrame* aFrame,
                             PRInt32 aNumChars, nsSize& aSize,
                             nsIRenderingContext *aRendContext);
#endif
  NS_IMETHOD GetSize(PRInt32* aSize) const;
  NS_IMETHOD GetMaxLength(PRInt32* aSize);

  static  void GetScrollBarDimensions(nsIPresContext& aPresContext,
                                  nscoord &aWidth, nscoord &aHeight);
  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

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

  // nsIPluggableEventListener
  NS_IMETHOD  PluggableEventHandler(nsIPresContext& aPresContext, 
                                    nsGUIEvent*     aEvent,
                                    nsEventStatus&  aEventStatus);

  NS_IMETHOD  PluggableGetFrameForPoint(const nsPoint& aPoint, 
                                        nsIFrame**     aFrame);

  // nsIListControlFrame
  NS_IMETHOD SetComboboxFrame(nsIFrame* aComboboxFrame);
  NS_IMETHOD GetSelectedItem(nsString & aStr);
  NS_IMETHOD AboutToDropDown();


  // Static Methods
  static nsIDOMHTMLSelectElement* GetSelect(nsIContent * aContent);
  static nsIDOMHTMLCollection*    GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect = nsnull);
  static nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRUint32 aIndex);
  static PRBool                   GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRUint32 aIndex, nsString& aValue);

protected:
  nsListControlFrame();
  virtual ~nsListControlFrame();

  nsIFrame * GetOptionFromChild(nsIFrame* aParentFrame);

  nsresult GetFrameForPointUsing(const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsIFrame**     aFrame);

  // nsHTMLContainerFrame overrides
  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

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
                             PRBool        aIsSelected);

  // Data Members
  nsFormFrame* mFormFrame;

  PRInt32      mNumRows;

  PRBool       mIsFrameSelected[64];
  PRInt32      mNumSelections;
  PRInt32      mMaxNumSelections;
  PRBool       mMultipleSelections;


  //nsIContent * mSelectedContent;
  PRInt32      mSelectedIndex;
  PRInt32      mStartExtendedIndex;
  PRInt32      mEndExtendedIndex;

  nsIFrame *   mHitFrame;
  nsIContent * mHitContent;

  nsIFrame *   mCurrentHitFrame;
  nsIContent * mCurrentHitContent;

  nsIFrame *   mSelectedFrame;
  nsIContent * mSelectedContent;

  PRBool       mIsInitializedFromContent;

  nsIFrame * mContentFrame;
  PRBool     mInDropDownMode;
  nsIComboboxControlFrame * mComboboxFrame;
  nsString   mSelectionStr;

};

#endif /* nsListControlFrame_h___ */

