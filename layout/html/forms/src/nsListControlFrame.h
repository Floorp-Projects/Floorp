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
#include "nsIFormControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"

class nsIDOMHTMLSelectElement;
class nsIDOMHTMLCollection;
class nsIDOMHTMLOptionElement;
class nsIComboboxControlFrame;
class nsIViewManager;
class nsIPresContext;

/**
 * Frame-based listbox.
 */

class nsListControlFrame : public nsScrollFrame, 
                           public nsIFormControlFrame, 
                           public nsIListControlFrame,
                           public nsIDOMMouseListener,
                           public nsIDOMMouseMotionListener,
                           public nsISelectControlFrame
{
public:
  friend nsresult NS_NewListControlFrame(nsIFrame** aNewFrame);

   // nsISupports
  NS_DECL_ISUPPORTS

    // nsIFrame
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext,
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);
  
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow);

  NS_IMETHOD DidReflow(nsIPresContext& aPresContext, nsDidReflowStatus aStatus);

    // nsIFormControlFrame
  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 
  NS_IMETHOD GetMultiple(PRBool* aResult, nsIDOMHTMLSelectElement* aSelect = nsnull);
  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);
  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);
  virtual void ScrollIntoView(nsIPresContext* aPresContext);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset();
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);
  virtual void SetFormFrame(nsFormFrame* aFrame);
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);

    // nsHTMLContainerFrame
  virtual PRIntn GetSkipSides() const;

    // nsIListControlFrame
  NS_IMETHOD SetComboboxFrame(nsIFrame* aComboboxFrame);
  NS_IMETHOD GetSelectedIndex(PRInt32* aIndex); 
  NS_IMETHOD GetSelectedItem(nsString & aStr);
  NS_IMETHOD CaptureMouseEvents(PRBool aGrabMouseEvents);
  NS_IMETHOD GetMaximumSize(nsSize &aSize);
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD GetNumberOfOptions(PRInt32* aNumOptions);  

  // nsISelectControlFrame
  NS_IMETHOD AddOption(PRInt32 index);
  NS_IMETHOD RemoveOption(PRInt32 index); 

  //nsIDOMEventListener
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);//{ printf("-MouseDown\n"); return NS_OK; };
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);// { printf("-MouseUp\n"); return NS_OK; };
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent)    { return NS_OK; }
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent)     { return NS_OK; }
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent)      { return NS_OK; }
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent)        { return NS_OK; }

  //nsIDOMEventMotionListener
  virtual nsresult MouseMove(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }


    // Static Methods
  static nsIDOMHTMLSelectElement* GetSelect(nsIContent * aContent);
  static nsIDOMHTMLCollection*    GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect = nsnull);
  static nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRUint32 aIndex);
  static nsIContent* GetOptionAsContent(nsIDOMHTMLCollection* aCollection,PRUint32 aIndex);
  static PRBool                   GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRUint32 aIndex, nsString& aValue);

protected:
  NS_IMETHOD GetSelectedIndexFromDOM(PRInt32* aIndex); // from DOM
 
  nsListControlFrame();
  virtual ~nsListControlFrame();

   // nsScrollFrame overrides
   // Override the widget created for the list box so a Borderless top level widget is created
   // for drop-down lists.
  virtual  nsresult CreateScrollingViewWidget(nsIView* aView,const nsStylePosition* aPosition);
  virtual  nsresult GetScrollingParentView(nsIFrame* aParent, nsIView** aParentView);
  PRInt32  GetNumberOfOptions();

    // Utility methods
  nsresult GetSizeAttribute(PRInt32 *aSize);
  PRInt32  GetNumberOfSelections();
  nsIContent* GetOptionFromContent(nsIContent *aContent);
  nsresult GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent);
  PRInt32  GetSelectedIndexFromContent(nsIContent *aContent);
  nsIContent* GetOptionContent(PRUint32 aIndex);
  PRBool   IsContentSelected(nsIContent* aContent);
  PRBool   IsContentSelectedByIndex(PRUint32 aIndex);
  void     SetContentSelected(PRUint32 aIndex, PRBool aSelected);
  void     GetViewOffset(nsIViewManager* aManager, nsIView* aView, nsPoint& aPoint);
  nsresult Deselect();
  nsIFrame *GetOptionFromChild(nsIFrame* aParentFrame);
  PRBool   IsAncestor(nsIView* aAncestor, nsIView* aChild);
  nsIView* GetViewFor(nsIWidget* aWidget);
  nsresult SyncViewWithFrame();
  PRBool   IsInDropDownMode();
  PRBool   IsOptionElement(nsIContent* aContent);
  PRBool   IsOptionElementFrame(nsIFrame *aFrame);
  nsIFrame *GetSelectableFrame(nsIFrame *aFrame);
  void     DisplaySelected(nsIContent* aContent); 
  void     DisplayDeselected(nsIContent* aContent); 
  void     ForceRedraw();
  PRBool   IsOptionGroup(nsIFrame* aFrame);
  void     SingleSelection();
  void     MultipleSelection(PRBool aIsShift, PRBool aIsControl);
  void     SelectIndex(PRInt32 aIndex); 
  void     ToggleSelected(PRInt32 aIndex);
  void     ClearSelection();
  void     ExtendedSelection(PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aDoInvert, PRBool aSetValue);

  PRBool   HasSameContent(nsIFrame* aFrame1, nsIFrame* aFrame2);
  void     HandleListSelection(nsIDOMEvent * aDOMEvent);
  PRInt32  GetSelectedIndexFromFrame(nsIFrame *aHitFrame);

  // Data Members
  nscoord      mBorderOffsetY;
  nsFormFrame* mFormFrame;
  PRInt32      mSelectedIndex;
  PRInt32      mOldSelectedIndex;
  PRInt32      mStartExtendedIndex;
  PRInt32      mEndExtendedIndex;
  nsIFrame*    mHitFrame;
  PRBool       mIsInitializedFromContent;
  nsIFrame*    mContentFrame;
  nsIComboboxControlFrame *mComboboxFrame;
  PRBool       mDisplayed;
  PRBool       mButtonDown;
  nsIFrame*    mLastFrame;
  nscoord      mMaxWidth;
  nscoord      mMaxHeight;
  PRBool       mIsCapturingMouseEvents;
  PRBool       mIgnoreMouseUp;             

  nsIPresContext* mPresContext;             // XXX: Remove the need to cache the pres context.

  // XXX temprary only until full system mouse capture works
  PRBool mIsScrollbarVisible;
};

#endif /* nsListControlFrame_h___ */

