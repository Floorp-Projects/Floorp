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

#ifndef nsComboboxControlFrame_h___
#define nsComboboxControlFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIComboboxControlFrame.h"
#ifdef PLUGGABLE_EVENTS
#include "nsIPluggableEventListener.h"
#endif

class nsButtonControlFrame;
class nsTextControlFrame;
class nsFormFrame;
class nsIView;
class nsStyleContext;
class nsIHTMLContent;
class nsIListControlFrame;

class nsComboboxControlFrame : public nsHTMLContainerFrame,
                               public nsIFormControlFrame,
                               public nsIComboboxControlFrame
#ifdef PLUGGABLE_EVENTS
                               public nsIPluggableEventListener
#endif
{
public:
  nsComboboxControlFrame();
  ~nsComboboxControlFrame();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
 
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aParentContext,
                                   PRInt32 aParentChange,
                                   nsStyleChangeList* aChangeList,
                                   PRInt32* aLocalChange);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameName(nsString& aResult) const;


  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

     // nsIFormControLFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  //nsTextControlFrame* GetTextFrame() { return mTextFrame; }

  //void SetTextFrame(nsTextControlFrame* aFrame) { mTextFrame = aFrame; }

  //nsButtonControlFrame* GetBrowseFrame() { return mBrowseFrame; }
  //void           SetBrowseFrame(nsButtonControlFrame* aFrame) { mBrowseFrame = aFrame; }
  void           SetFocus(PRBool aOn, PRBool aRepaint);
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual void   SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }
  virtual void   Reset();
  NS_IMETHOD     GetName(nsString* aName);
  NS_IMETHOD     GetType(PRInt32* aType) const;

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);
  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);



  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

  nsresult GetFrameForPointUsing(const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsIFrame**     aFrame);
  // A Static Helper Functions

  // This refreshes a particular pseudo style content when a "ReResolveStyleContent" happens
  static void RefreshStyleContext(nsIPresContext* aPresContext,
                                  nsIAtom *         aNewContentPseudo,
                                  nsIStyleContext*& aCurrentStyle,
                                  nsIContent *      aContent,
                                  nsIStyleContext*  aParentStyle);

  // nsIFormMouseListener
  virtual void MouseClicked(nsIPresContext* aPresContext);

  // nsIPluggableEventListener
  NS_IMETHOD  PluggableEventHandler(nsIPresContext& aPresContext, 
                                    nsGUIEvent*     aEvent,
                                    nsEventStatus&  aEventStatus);

  NS_IMETHOD  PluggableGetFrameForPoint(const nsPoint& aPoint, 
                                        nsIFrame**     aFrame);

  //static PRInt32 gSpacing;

  //nsIComboboxControlFrame
  NS_IMETHOD SetDropDown(nsIFrame* aPlaceHolderFrame, nsIFrame* aDropDownFrame);
  NS_IMETHOD SetDropDownStyleContexts(nsIStyleContext * aVisible, nsIStyleContext * aHidden);
  NS_IMETHOD SetButtonStyleContexts(nsIStyleContext * aOut, nsIStyleContext * aPressed);
  NS_IMETHOD ListWasSelected(nsIPresContext* aPresContext);


protected:
  virtual void PaintComboboxControl(nsIPresContext& aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    const nsRect& aDirtyRect,
                                    nsFramePaintLayer aWhichLayer);
  nsIWidget* GetWindowTemp(nsIView *aView); // XXX temporary
  virtual PRIntn GetSkipSides() const;


  nsFormFrame*          mFormFrame;               // Parent Form Frame

  nsIFrame *            mPlaceHolderFrame;
  nsIFrame *            mListFrame;               // Generic nsIFrame
  nsIListControlFrame * mListControlFrame;        // Specific ListControl Interface
  nsRect                mButtonRect;              // The Arrow Button's Rect cached for Painting
 
  // DropDown List Visibility Styles
  nsIStyleContext * mVisibleStyleContext;         // Style for the DropDown List to make it visible
  nsIStyleContext * mHiddenStyleContext;          // Style for the DropDown List to make it hidden
  nsIStyleContext * mCurrentStyleContext;         // The current Style state of the DropDown List

  // Arrow Button Styles 
  nsIStyleContext * mBtnOutStyleContext;          // Style when not pressed
  nsIStyleContext * mBtnPressedStyleContext;      // Style When Pressed
  nsIStyleContext * mArrowStyle;                  // Arrows currrent style (for painting)

  // Combobox Text Styles
  nsIStyleContext * mBlockTextStyle;              // Style when there is no selection and it doesn't have focus
  nsIStyleContext * mBlockTextSelectedStyle;      // Style when selected and it doesn't have focus
  nsIStyleContext * mBlockTextSelectedFocusStyle; // Style when selected and it has focus


  PRBool            mFirstTime; 

  // State data members
  nsString          mTextStr;
  PRBool            mGotFocus;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

#endif


