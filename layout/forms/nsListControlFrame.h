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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef nsListControlFrame_h___
#define nsListControlFrame_h___

#ifdef DEBUG_evaughan
//#define DEBUG_rods
#endif

#ifdef DEBUG_rods
//#define DO_REFLOW_DEBUG
//#define DO_REFLOW_COUNTER
//#define DO_UNCONSTRAINED_CHECK
//#define DO_PIXELS
#endif

#include "nsScrollFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIStatefulFrame.h"
#include "nsIPresState.h"
#include "nsCWeakReference.h"
#include "nsIContent.h"

class nsIDOMHTMLSelectElement;
class nsIDOMHTMLCollection;
class nsIDOMHTMLOptionElement;
class nsIComboboxControlFrame;
class nsIViewManager;
class nsIPresContext;
class nsVoidArray;
class nsIScrollableView;

class nsListControlFrame;
class nsSelectUpdateTimer;
class nsVoidArray;

#define NS_ILIST_EVENT_LISTENER_IID \
{/* 45BC6821-6EFB-11d4-B1EE-000064657374*/ \
0x45bc6821, 0x6efb, 0x11d4, \
{0xb1, 0xee, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/******************************************************************************
 * nsIListEventListener
 * Standard interface for event listeners that are attached to nsListControls
 ******************************************************************************/
 
class nsIListEventListener : public nsISupports
{
public:
 
  static const nsIID& GetIID() { static nsIID iid = NS_ILIST_EVENT_LISTENER_IID; return iid; }
 
  /** SetFrame sets the frame we send event messages to, when necessary
   *  @param aFrame -- the frame, can be null, not ref counted (guaranteed to outlive us!)
   */
  NS_IMETHOD SetFrame(nsListControlFrame *aFrame)=0;

};
/******************************************************************************
 * nsListEventListener
 * This class is responsible for propogating events to the nsListControlFrame
 * becuase it isn't ref-counted
 ******************************************************************************/

class nsListEventListener; // forward declaration for factory

/* factory for ender key listener */
nsresult NS_NewListEventListener(nsIListEventListener ** aInstancePtrResult);

class nsListEventListener : public nsIListEventListener,
                            public nsIDOMKeyListener, 
                            public nsIDOMMouseListener,
                            public nsIDOMMouseMotionListener

{
public:

  /** the default destructor */
  virtual ~nsListEventListener();

  /** interfaces for addref and release and queryinterface*/
  NS_DECL_ISUPPORTS

  /** nsIDOMKeyListener interfaces 
    * @see nsIDOMKeyListener
    */
  NS_IMETHOD SetFrame(nsListControlFrame *aFrame);

  /** nsIDOMKeyListener interfaces 
    * @see nsIDOMKeyListener
    */
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
  /* END interfaces from nsIDOMKeyListener*/

  /** nsIDOMMouseListener interfaces 
    * @see nsIDOMMouseListener
    */
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
  /* END interfaces from nsIDOMMouseListener*/

  //nsIDOMEventMotionListener
  /** nsIDOMEventMotionListener interfaces 
    * @see nsIDOMEventMotionListener
    */
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent);
  /* END interfaces from nsIDOMMouseListener*/
  
  friend nsresult NS_NewListEventListener(nsIListEventListener ** aInstancePtrResult);

protected:
  /** the default constructor.  Protected, use the factory to create an instance.
    * @see NS_NewEnderEventListener
    */
  nsListEventListener();

protected:
  nsCWeakReference<nsListControlFrame> mFrame;
  nsCOMPtr<nsIContent>      mContent; // ref counted
};

/**
 * Frame-based listbox.
 */

class nsListControlFrame : public nsScrollFrame, 
                           public nsIFormControlFrame, 
                           public nsIListControlFrame,
                           public nsIDOMMouseListener,
                           public nsIDOMMouseMotionListener,
                           public nsIDOMKeyListener,
                           public nsISelectControlFrame
{
public:
  friend nsresult NS_NewListControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
  friend class nsSelectUpdateTimer;

   // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

    // nsIFrame
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext,
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);
  
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow);

  NS_IMETHOD DidReflow(nsIPresContext* aPresContext, nsDidReflowStatus aStatus);
  NS_IMETHOD MoveTo(nsIPresContext* aPresContext, nscoord aX, nscoord aY);
  NS_IMETHOD Destroy(nsIPresContext *aPresContext);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::scrollFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

    // nsIFormControlFrame
  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue); 
  NS_IMETHOD GetMultiple(PRBool* aResult, nsIDOMHTMLSelectElement* aSelect = nsnull);
  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                     const nsFont*&  aFont);
  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;

  virtual void SetFocus(PRBool aOn = PR_TRUE, PRBool aRepaint = PR_FALSE);
  virtual void ScrollIntoView(nsIPresContext* aPresContext);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset(nsIPresContext* aPresContext) { ResetList(aPresContext); }
  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);
  virtual void SetFormFrame(nsFormFrame* aFrame);
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);

    // for accessibility purposes
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

    // nsHTMLContainerFrame
  virtual PRIntn GetSkipSides() const;

    // nsIListControlFrame
  NS_IMETHOD SetComboboxFrame(nsIFrame* aComboboxFrame);
  NS_IMETHOD GetSelectedIndex(PRInt32* aIndex); 
  NS_IMETHOD GetSelectedItem(nsString & aStr);
  NS_IMETHOD CaptureMouseEvents(nsIPresContext* aPresContext, PRBool aGrabMouseEvents);
  NS_IMETHOD GetMaximumSize(nsSize &aSize);
  NS_IMETHOD SetSuggestedSize(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD GetNumberOfOptions(PRInt32* aNumOptions);  
  NS_IMETHOD SyncViewWithFrame(nsIPresContext* aPresContext);
  NS_IMETHOD AboutToDropDown();
  NS_IMETHOD AboutToRollup();
  NS_IMETHOD UpdateSelection(PRBool aDoDispatchEvent, PRBool aForceUpdate, nsIContent* aContent);
  NS_IMETHOD SetPresState(nsIPresState * aState) { mPresState = aState; return NS_OK;}
  NS_IMETHOD SetOverrideReflowOptimization(PRBool aValue) { mOverrideReflowOpt = aValue; return NS_OK; }
  NS_IMETHOD GetOptionsContainer(nsIPresContext* aPresContext, nsIFrame** aFrame);

  NS_IMETHOD SaveStateInternal(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreStateInternal(nsIPresContext* aPresContext, nsIPresState* aState);

  // nsISelectControlFrame
  NS_IMETHOD AddOption(nsIPresContext* aPresContext, PRInt32 index);
  NS_IMETHOD RemoveOption(nsIPresContext* aPresContext, PRInt32 index);
  NS_IMETHOD SetOptionSelected(PRInt32 aIndex, PRBool aValue);
  NS_IMETHOD GetOptionSelected(PRInt32 aIndex, PRBool* aValue);
  NS_IMETHOD DoneAddingContent(PRBool aIsDone);
  NS_IMETHOD OptionDisabled(nsIContent * aContent);
  NS_IMETHOD MakeSureSomethingIsSelected(nsIPresContext* aPresContext) { return NS_OK; }

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

  //nsIDOMEventListener
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent)    { return NS_OK; }
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; }
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent)     { return NS_OK; }
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent)      { return NS_OK; }
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent)        { return NS_OK; }

  //nsIDOMEventMotionListener
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent);

  //nsIDOMKeyListener
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent)    { return NS_OK; }
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

    // Static Methods
  static nsIDOMHTMLSelectElement* GetSelect(nsIContent * aContent);
  static nsIDOMHTMLCollection*    GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect = nsnull);
  static nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRInt32 aIndex);
  static nsIContent* GetOptionAsContent(nsIDOMHTMLCollection* aCollection,PRInt32 aIndex);
  static PRBool                   GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRInt32 aIndex, nsString& aValue);

  // Weak Reference
  nsCWeakReferent *WeakReferent()
    { return &mWeakReferent; }

  // Helper 
  void SetPassId(PRInt16 aId)  { mPassId = aId; }

protected:

  NS_IMETHOD GetSelectedIndexFromDOM(PRInt32* aIndex); // from DOM
  NS_IMETHOD IsTargetOptionDisabled(PRBool &aIsDisabled);
  NS_IMETHOD IsOptionDisabled(PRInt32 anIndex, PRBool &aIsDisabled);
  nsresult   ScrollToFrame(nsIContent * aOptElement);
  PRBool     IsClickingInCombobox(nsIDOMEvent* aMouseEvent);
  void       AdjustIndexForDisabledOpt(PRInt32 &anNewIndex, PRInt32 &anOldIndex, 
                                       PRBool &aDoSetNewIndex, PRBool &aWasDisabled,
                                       PRInt32 aNumOptions, PRInt32 aDoAdjustInc, PRInt32 aDoAdjustIncNext);
  virtual void ResetList(nsIPresContext* aPresContext, nsVoidArray * aInxList = nsnull);

  // PresState Helper Methods
  nsresult   GetPresStateAndValueArray(nsISupportsArray ** aSuppArray);
  nsresult   SetOptionIntoPresState(nsISupportsArray * aSuppArray, 
                                    PRInt32            aIndex, 
                                    PRInt32            anItemNum);
  nsresult   SetSelectionInPresState(PRInt32 aIndex, PRBool aValue);
  nsresult   RemoveOptionFromPresState(nsISupportsArray * aSuppArray, 
                                       PRInt32            aIndex);

  nsListControlFrame();
  virtual ~nsListControlFrame();

   // nsScrollFrame overrides
   // Override the widget created for the list box so a Borderless top level widget is created
   // for drop-down lists.
  virtual  nsresult CreateScrollingViewWidget(nsIView* aView, const nsStyleDisplay* aDisplay);
  virtual  nsresult GetScrollingParentView(nsIPresContext* aPresContext,
                                           nsIFrame* aParent,
                                           nsIView** aParentView);
  PRInt32  GetNumberOfOptions();

    // Utility methods
  nsresult GetSizeAttribute(PRInt32 *aSize);
  PRInt32  GetNumberOfSelections();
  nsIContent* GetOptionFromContent(nsIContent *aContent);
  nsresult GetIndexFromDOMEvent(nsIDOMEvent* aMouseEvent, PRInt32& aOldIndex, PRInt32& aCurIndex);
  PRInt32  GetSelectedIndexFromContent(nsIContent *aContent);
  nsIContent* GetOptionContent(PRInt32 aIndex);
  PRBool   IsContentSelected(nsIContent* aContent);
  PRBool   IsContentSelectedByIndex(PRInt32 aIndex);
  void     SetContentSelected(PRInt32 aIndex, 
                              PRBool         aSelected,
                              PRBool         aDoScrollTo = PR_TRUE,
                              nsIPresShell * aPresShell  = nsnull);
  void     SetContentSelected(PRInt32        aIndex,
                              nsIContent *   aContent, 
                              PRBool         aSelected,
                              PRBool         aDoScrollTo = PR_TRUE,
                              nsIPresShell * aPresShell  = nsnull);
  void     GetViewOffset(nsIViewManager* aManager, nsIView* aView, nsPoint& aPoint);
  nsresult Deselect();
  nsIFrame *GetOptionFromChild(nsIFrame* aParentFrame);
  PRBool   IsAncestor(nsIView* aAncestor, nsIView* aChild);
  nsIView* GetViewFor(nsIWidget* aWidget);
  PRBool   IsInDropDownMode();
  PRBool   IsOptionElement(nsIContent* aContent);
  PRBool   IsOptionElementFrame(nsIFrame *aFrame);
  nsIFrame *GetSelectableFrame(nsIFrame *aFrame);
  void     DisplaySelected(nsIContent* aContent); 
  void     DisplayDeselected(nsIContent* aContent); 
  void     ForceRedraw(nsIPresContext* aPresContext);
  PRBool   IsOptionGroup(nsIFrame* aFrame);
  void     SingleSelection();
  void     MultipleSelection(PRBool aIsShift, PRBool aIsControl);
  void     SelectIndex(PRInt32 aIndex); 
  void     ToggleSelected(PRInt32 aIndex);
  void     ClearSelection();
  void     ExtendedSelection(PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aDoInvert, PRBool aSetValue);
  void     ResetSelectedItem();
  PRBool   CheckIfAllFramesHere();

  PRBool   HasSameContent(nsIFrame* aFrame1, nsIFrame* aFrame2);
  void     HandleListSelection(nsIDOMEvent * aDOMEvent);
  PRInt32  GetSelectedIndexFromFrame(nsIFrame *aHitFrame);
  PRBool   IsLeftButton(nsIDOMEvent* aMouseEvent);

  void     GetScrollableView(nsIScrollableView*& aScrollableView);

  // Timer Methods
  nsresult StartUpdateTimer(nsIPresContext * aPresContext);
  void     StopUpdateTimer();
  void     ItemsHaveBeenRemoved(nsIPresContext * aPresContext);

  // onChange detection
  nsresult SelectionChanged(nsIContent* aContent);
  
  // Data Members
  nsFormFrame* mFormFrame;
  PRInt32      mSelectedIndex;
  PRInt32      mOldSelectedIndex;
  PRInt32      mSelectedIndexWhenPoppedDown;
  PRInt32      mStartExtendedIndex;
  PRInt32      mEndExtendedIndex;
  PRPackedBool mIsInitializedFromContent;
  nsIComboboxControlFrame *mComboboxFrame;
  PRPackedBool mButtonDown;
  nscoord      mMaxWidth;
  nscoord      mMaxHeight;
  PRPackedBool mIsCapturingMouseEvents;
  PRInt32      mNumDisplayRows;

  nsVoidArray  * mSelectionCache;
  PRInt32        mSelectionCacheLength;

  PRBool       mIsAllContentHere;
  PRPackedBool mIsAllFramesHere;
  PRPackedBool mHasBeenInitialized;
  PRPackedBool mDoneWithInitialReflow;

  PRPackedBool mOverrideReflowOpt;

  PRInt32      mDelayedIndexSetting;
  PRPackedBool mDelayedValueSetting;

  nsIPresContext* mPresContext;             // XXX: Remove the need to cache the pres context.

  nsCOMPtr<nsIPresState> mPresState;        // Need cache state when list is null

  nsCOMPtr<nsIListEventListener> mEventListener;           // ref counted
  nsCWeakReferent mWeakReferent; // so this obj can be used as a weak ptr

  // XXX temprary only until full system mouse capture works
  PRPackedBool mIsScrollbarVisible;

  PRInt16 mPassId;
  nsSize mCachedDesiredMaxSize;

  // Update timer
  nsSelectUpdateTimer * mUpdateTimer;

  //Resize Reflow OpitmizationSize;
  nsSize       mCacheSize;
  nsSize       mCachedMaxElementSize;
  nsSize       mCachedUnconstrainedSize;
  nsSize       mCachedAvailableSize;

#ifdef DO_REFLOW_COUNTER
  PRInt32 mReflowId;
#endif

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

#endif /* nsListControlFrame_h___ */

