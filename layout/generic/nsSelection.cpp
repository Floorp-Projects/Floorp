/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/*
 * Implementation of selection: nsISelection,nsISelectionPrivate and nsFrameSelection
 */

#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsIFactory.h"
#include "nsIEnumerator.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsFrameSelection.h"
#include "nsISelection.h"
#include "nsISelection2.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionListener.h"
#include "nsIComponentManager.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsRange.h"
#include "nsCOMArray.h"
#include "nsGUIEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsITableLayout.h"
#include "nsITableCellLayout.h"
#include "nsIDOMNodeList.h"
#include "nsTArray.h"

#include "nsISelectionListener.h"
#include "nsIContentIterator.h"
#include "nsIDocumentEncoder.h"

// for IBMBIDI
#include "nsFrameTraversal.h"
#include "nsILineIterator.h"
#include "nsGkAtoms.h"
#include "nsIFrameTraversal.h"
#include "nsLayoutUtils.h"
#include "nsLayoutCID.h"
#include "nsBidiPresUtils.h"
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);

#include "nsIDOMText.h"

#include "nsContentUtils.h"
#include "nsThreadUtils.h"

//included for desired x position;
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsCaret.h"


// included for view scrolling
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsITimer.h"
#include "nsIServiceManager.h"
#include "nsFrameManager.h"
// notifications
#include "nsIDOMDocument.h"
#include "nsIDocument.h"

#include "nsISelectionController.h"//for the enums
#include "nsAutoCopyListener.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"

#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#endif // IBMBIDI

//#define DEBUG_TABLE 1

static NS_DEFINE_IID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kCSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);

//PROTOTYPES
class nsSelectionIterator;
class nsFrameSelection;
class nsAutoScrollTimer;

static PRBool IsValidSelectionPoint(nsFrameSelection *aFrameSel, nsINode *aNode);

static nsIAtom *GetTag(nsINode *aNode);
// returns the parent
static nsINode* ParentOffset(nsINode *aNode, PRInt32 *aChildOffset);
static nsINode* GetCellParent(nsINode *aDomNode);

#ifdef PRINT_RANGE
static void printRange(nsIRange *aDomRange);
#define DEBUG_OUT_RANGE(x)  printRange(x)
#else
#define DEBUG_OUT_RANGE(x)  
#endif //MOZ_DEBUG



//#define DEBUG_SELECTION // uncomment for printf describing every collapse and extend.
//#define DEBUG_NAVIGATION


//#define DEBUG_TABLE_SELECTION 1

struct CachedOffsetForFrame {
  CachedOffsetForFrame()
  : mCachedFrameOffset(0, 0) // nsPoint ctor
  , mLastCaretFrame(nsnull)
  , mLastContentOffset(0)
  , mCanCacheFrameOffset(PR_FALSE)
  {}

  nsPoint      mCachedFrameOffset;      // cached frame offset
  nsIFrame*    mLastCaretFrame;         // store the frame the caret was last drawn in.
  PRInt32      mLastContentOffset;      // store last content offset
  PRPackedBool mCanCacheFrameOffset;    // cached frame offset is valid?
};

struct RangeData
{
  RangeData(nsIRange* aRange, PRInt32 aEndIndex) :
    mRange(aRange), mEndIndex(aEndIndex) {}

  nsCOMPtr<nsIRange> mRange;
  PRInt32 mEndIndex; // index into mRangeEndings of this item
};

static RangeData sEmptyData(nsnull, 0);

// Note, the ownership of nsTypedSelection depends on which way the object is
// created. When nsFrameSelection has created nsTypedSelection,
// addreffing/releasing nsTypedSelection object is aggregated to
// nsFrameSelection. Otherwise normal addref/release is used.
// This ensures that nsFrameSelection is never deleted before its
// nsTypedSelections.

class nsTypedSelection : public nsISelection2,
                         public nsISelectionPrivate,
                         public nsSupportsWeakReference
{
public:
  nsTypedSelection();
  nsTypedSelection(nsFrameSelection *aList);
  virtual ~nsTypedSelection();
  
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTypedSelection, nsISelection)
  NS_DECL_NSISELECTION
  NS_DECL_NSISELECTION2
  NS_DECL_NSISELECTIONPRIVATE

  // utility methods for scrolling the selection into view
  nsresult      GetPresContext(nsPresContext **aPresContext);
  nsresult      GetPresShell(nsIPresShell **aPresShell);
  nsresult      GetRootScrollableView(nsIScrollableView **aScrollableView);
  nsresult      GetFrameToScrolledViewOffsets(nsIScrollableView *aScrollableView, nsIFrame *aFrame, nscoord *aXOffset, nscoord *aYOffset);
  nsresult      GetPointFromOffset(nsIFrame *aFrame, PRInt32 aContentOffset, nsPoint *aPoint);
  nsresult      GetSelectionRegionRectAndScrollableView(SelectionRegion aRegion, nsRect *aRect, nsIScrollableView **aScrollableView);
  nsresult      ScrollRectIntoView(nsIScrollableView *aScrollableView, nsRect& aRect, PRIntn  aVPercent, PRIntn  aHPercent, PRBool aScrollParentViews);

  nsresult      PostScrollSelectionIntoViewEvent(SelectionRegion aRegion);
  // aDoFlush only matters if aIsSynchronous is true.  If not, we'll just flush
  // when the scroll event fires so we make sure to scroll to the right place.
  nsresult      ScrollIntoView(SelectionRegion aRegion, PRBool aIsSynchronous,
                               PRBool aDoFlush,
                               PRInt16 aVPercent = NS_PRESSHELL_SCROLL_ANYWHERE,
                               PRInt16 aHPercent = NS_PRESSHELL_SCROLL_ANYWHERE);
  nsresult      AddItem(nsIRange *aRange, PRInt32* aOutIndex = nsnull);
  nsresult      RemoveItem(nsIRange *aRange);
  nsresult      RemoveCollapsedRanges();
  nsresult      Clear(nsPresContext* aPresContext);
  nsresult      Collapse(nsINode* aParentNode, PRInt32 aOffset);
  nsresult      Extend(nsINode* aParentNode, PRInt32 aOffset);
  nsresult      AddRange(nsIRange* aRange);
  // The nsIRange version of RemoveRange assumes the caller is holding
  // a strong reference to aRange.
  nsresult      RemoveRange(nsIRange* aRange);
  nsIRange*     GetRangeAt(PRInt32 aIndex);
  nsresult      GetTableSelectionType(nsIRange* aRange,
                                      PRInt32* aTableSelectionType);

  // methods for convenience. Note, these don't addref
  nsINode*     GetAnchorNode();
  PRInt32      GetAnchorOffset();

  nsINode*     GetFocusNode();
  PRInt32      GetFocusOffset();

  // Get the anchor-to-focus range if we don't care which end is
  // anchor and which end is focus.
  const nsIRange* GetAnchorFocusRange() const {
    return mAnchorFocusRange;
  }

  nsDirection  GetDirection(){return mDirection;}
  void         SetDirection(nsDirection aDir){mDirection = aDir;}
  nsresult     CopyRangeToAnchorFocus(nsIRange *aRange);
  void         ReplaceAnchorFocusRange(nsIRange *aRange);

  //  NS_IMETHOD   GetPrimaryFrameForRangeEndpoint(nsIDOMNode *aNode, PRInt32 aOffset, PRBool aIsEndNode, nsIFrame **aResultFrame);
  NS_IMETHOD   GetPrimaryFrameForAnchorNode(nsIFrame **aResultFrame);
  NS_IMETHOD   GetPrimaryFrameForFocusNode(nsIFrame **aResultFrame, PRInt32 *aOffset, PRBool aVisual);
  NS_IMETHOD   LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, SelectionType aType, PRBool aSlowCheck);
  NS_IMETHOD   Repaint(nsPresContext* aPresContext);

  // Note: StartAutoScrollTimer might destroy arbitrary frames, views etc.
  nsresult     StartAutoScrollTimer(nsPresContext *aPresContext,
                                    nsIView *aView,
                                    nsPoint& aPoint,
                                    PRUint32 aDelay);

  nsresult     StopAutoScrollTimer();


private:
  friend class nsAutoScrollTimer;

  // Note: DoAutoScrollView might destroy arbitrary frames, views etc.
  nsresult DoAutoScrollView(nsPresContext *aPresContext,
                            nsIView *aView,
                            nsPoint& aPoint,
                            PRBool aScrollParentViews);

  // Note: ScrollPointIntoClipView might destroy arbitrary frames, views etc.
  nsresult     ScrollPointIntoClipView(nsPresContext *aPresContext, nsIView *aView, nsPoint& aPoint, PRBool *aDidScroll);
  // Note: ScrollPointIntoView might destroy arbitrary frames, views etc.
  nsresult     ScrollPointIntoView(nsPresContext *aPresContext, nsIView *aView, nsPoint& aPoint, PRBool aScrollParentViews, PRBool *aDidScroll);
  nsresult     GetViewAncestorOffset(nsIView *aView, nsIView *aAncestorView, nscoord *aXOffset, nscoord *aYOffset);

public:
  SelectionType GetType(){return mType;}
  void          SetType(SelectionType aType){mType = aType;}

  nsresult     NotifySelectionListeners();

private:
  friend class nsSelectionIterator;

  class ScrollSelectionIntoViewEvent;
  friend class ScrollSelectionIntoViewEvent;

  class ScrollSelectionIntoViewEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    ScrollSelectionIntoViewEvent(nsTypedSelection *aTypedSelection,
                                 SelectionRegion aRegion) 
      : mTypedSelection(aTypedSelection),
        mRegion(aRegion) {
      NS_ASSERTION(aTypedSelection, "null parameter");
    }
    void Revoke() { mTypedSelection = nsnull; }
  private:
    nsTypedSelection *mTypedSelection;
    SelectionRegion   mRegion;
  };

  void setAnchorFocusRange(PRInt32 aIndex); // pass in index into mRanges;
                                            // negative value clears
                                            // mAnchorFocusRange
  nsresult     selectFrames(nsPresContext* aPresContext, nsIContentIterator *aInnerIter, nsIContent *aContent, nsIPresShell *aPresShell, PRBool aFlags);
  nsresult     selectFrames(nsPresContext* aPresContext, nsIRange *aRange, PRBool aSelect);
  nsresult     getTableCellLocationFromRange(nsIRange *aRange, PRInt32 *aSelectionType, PRInt32 *aRow, PRInt32 *aCol);
  nsresult     addTableCellRange(nsIRange *aRange, PRBool *aDidAddRange, PRInt32 *aOutIndex);
  
  // These are the ranges inside this selection. They are kept sorted in order
  // of DOM position of start and end, respectively (both of these arrays
  // should have the same contents, but possibly in different orders).
  //
  // This data structure is sorted by the range beginnings and the range
  // endings. When searching for a range, we can discard ranges whose ends
  // are before the point in question, or whose beginnings are after. We can
  // find these two sets of ranges on O(log n) time.
  //
  // Merging these two result sets takes O(n) time, so a a full query is O(log
  // n + n) time. This looks worse than brute force O(n) searching, but in
  // practice it is much faster. The DOM comparisons used in a brute force
  // search are VERY expensive because they actually walk the DOM tree. We only
  // do log(n) of these comparisons.
  //
  // Our O(n) merging step uses very fast integer comparisons, and, since
  // we store the range ending index in the structure, we have to merge at
  // most half of the results. Timing shows that this algorithm is nearly
  // twice as fast doing intersections for 9 ranges, and 18 times faster for
  // 250 ranges.
  //
  // An interval tree would give us O(log n) time lookups, which would be
  // better. However, this approach gets us most of the way, and doesn't
  // require rebalancing and other  overhead. If this algorithm is found to be
  // a bottleneck, it should be replaced with an interval tree.
  //
  // It has been discussed requiring selections to be disjoint in the future.
  // If this requirement is added, the current design makes the most sense, we
  // can just remove the array sorted by endings.

#ifdef DEBUG
  PRBool ValidateRanges();
#endif

  PRInt32 FindInsertionPoint(
      const nsTArray<PRInt32>* aRemappingArray,
      nsINode* aPointNode, PRInt32 aPointOffset,
      PRInt32 (*aComparator)(nsINode*,PRInt32,nsIRange*));
  void MoveIndexToFirstMatch(PRInt32* aIndex, nsINode* aNode,
                             PRInt32 aOffset,
                             const nsTArray<PRInt32>* aArray,
                             PRBool aUseBeginning);
  void MoveIndexToNextMismatch(PRInt32* aIndex, nsINode* aNode,
                               PRInt32 aOffset,
                               const nsTArray<PRInt32>* aRemappingArray,
                               PRBool aUseBeginning);
  PRInt32 FindRangeGivenPoint(nsINode* aBeginNode, PRInt32 aBeginOffset,
                              nsINode* aEndNode, PRInt32 aEndOffset,
                              PRInt32 aStartSearchingHere);
  nsresult GetRangesForIntervalCOMArray(nsINode* aBeginNode, PRInt32 aBeginOffset,
                                        nsINode* aEndNode, PRInt32 aEndOffset,
                                        PRBool aAllowAdjacent,
                                        nsCOMArray<nsIRange>* aRanges);

  nsTArray<RangeData> mRanges;
  nsTArray<PRInt32> mRangeEndings;    // references info mRanges
  nsCOMPtr<nsIRange> mAnchorFocusRange;
  nsRefPtr<nsFrameSelection> mFrameSelection;
  nsWeakPtr mPresShellWeak;
  nsRefPtr<nsAutoScrollTimer> mAutoScrollTimer;
  nsCOMArray<nsISelectionListener> mSelectionListeners;
  nsRevocableEventPtr<ScrollSelectionIntoViewEvent> mScrollEvent;
  CachedOffsetForFrame *mCachedOffsetForFrame;
  nsDirection mDirection;
  SelectionType mType;
};

// Stack-class to turn on/off selection batching for table selection
class NS_STACK_CLASS NS_FINAL_CLASS nsSelectionBatcher
{
private:
  nsCOMPtr<nsISelectionPrivate> mSelection;
public:
  nsSelectionBatcher(nsISelectionPrivate *aSelection) : mSelection(aSelection)
  {
    if (mSelection) mSelection->StartBatchChanges();
  }
  ~nsSelectionBatcher() 
  { 
    if (mSelection) mSelection->EndBatchChanges();
  }
};

class nsSelectionIterator : public nsIBidirectionalEnumerator
{
public:
/*BEGIN nsIEnumerator interfaces
see the nsIEnumerator for more details*/

  NS_DECL_ISUPPORTS

  NS_DECL_NSIENUMERATOR

  NS_DECL_NSIBIDIRECTIONALENUMERATOR

/*END nsIEnumerator interfaces*/
/*BEGIN Helper Methods*/
  nsIRange* CurrentItem();
/*END Helper Methods*/
private:
  friend class nsTypedSelection;

  //lame lame lame if delete from document goes away then get rid of this unless its debug
  friend class nsFrameSelection;

  nsSelectionIterator(nsTypedSelection *);
  virtual ~nsSelectionIterator();
  PRInt32     mIndex;
  nsTypedSelection *mDomSelection;
  SelectionType mType;
};

class nsAutoScrollTimer : public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsAutoScrollTimer()
  : mFrameSelection(0), mSelection(0), mPresContext(0), mPoint(0,0), mDelay(30)
  {
  }

  virtual ~nsAutoScrollTimer()
  {
   if (mTimer)
       mTimer->Cancel();
  }

  nsresult Start(nsPresContext *aPresContext, nsIView *aView, nsPoint &aPoint)
  {
    mPoint = aPoint;

    // Store the presentation context. The timer will be
    // stopped by the selection if the prescontext is destroyed.
    mPresContext = aPresContext;

    // Store the content from the nearest capturing frame. If this returns null
    // the capturing frame is the root.
    nsIFrame* clientFrame = static_cast<nsIFrame*>(aView->GetClientData());
    NS_ASSERTION(clientFrame, "Missing client frame");

    nsIFrame* capturingFrame = nsFrame::GetNearestCapturingFrame(clientFrame);
    NS_ASSERTION(!capturingFrame || capturingFrame->GetMouseCapturer(),
                 "Capturing frame should have a mouse capturer" );

    NS_ASSERTION(!capturingFrame || mPresContext == capturingFrame->PresContext(),
                 "Shouldn't have different pres contexts");

    NS_ASSERTION(capturingFrame != mPresContext->PresShell()->FrameManager()->GetRootFrame(),
                 "Capturing frame should not be the root frame");

    if (capturingFrame)
    {
      mContent = capturingFrame->GetContent();
      NS_ASSERTION(mContent, "Need content");

      NS_ASSERTION(mContent != mPresContext->PresShell()->FrameManager()->GetRootFrame()->GetContent(),
                 "We didn't want the root content!");

      NS_ASSERTION(capturingFrame == nsFrame::GetNearestCapturingFrame(
                   mPresContext->PresShell()->GetPrimaryFrameFor(mContent)),
                   "Mapping of frame to content failed.");
    }

    // Check that if there was no capturing frame the content is null.
    NS_ASSERTION(capturingFrame || !mContent, "Content not cleared correctly.");

    if (!mTimer)
    {
      nsresult result;
      mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);

      if (NS_FAILED(result))
        return result;
    }

    return mTimer->InitWithCallback(this, mDelay, nsITimer::TYPE_ONE_SHOT);
  }

  nsresult Stop()
  {
    if (mTimer)
    {
      mTimer->Cancel();
      mTimer = 0;
    }

    mContent = nsnull;
    return NS_OK;
  }

  nsresult Init(nsFrameSelection *aFrameSelection, nsTypedSelection *aSelection)
  {
    mFrameSelection = aFrameSelection;
    mSelection = aSelection;
    return NS_OK;
  }

  nsresult SetDelay(PRUint32 aDelay)
  {
    mDelay = aDelay;
    return NS_OK;
  }

  NS_IMETHOD Notify(nsITimer *timer)
  {
    if (mSelection && mPresContext)
    {
      // If the content is null the capturing frame must be the root frame.
      nsIFrame* capturingFrame;
      if (mContent)
      {
        nsIFrame* contentFrame = mPresContext->PresShell()->GetPrimaryFrameFor(mContent);
        if (contentFrame)
        {
          capturingFrame = nsFrame::GetNearestCapturingFrame(contentFrame);
        }
        else 
        {
          capturingFrame = nsnull;
        }
        NS_ASSERTION(!capturingFrame || capturingFrame->GetMouseCapturer(),
                     "Capturing frame should have a mouse capturer" );
      }
      else
      {
        capturingFrame = mPresContext->PresShell()->FrameManager()->GetRootFrame();
      }

      // Clear the content reference now that the frame has been found.
      mContent = nsnull;

      // This could happen for a frame with style changed to display:none or a frame
      // that was destroyed.
      if (!capturingFrame) {
        NS_WARNING("Frame destroyed or set to display:none before scroll timer fired.");
        return NS_OK;
      }

      nsIView* captureView = capturingFrame->GetMouseCapturer();
    
      nsWeakFrame viewFrame = static_cast<nsIFrame*>(captureView->GetClientData());
      NS_ASSERTION(viewFrame.GetFrame(), "View must have a client frame");
      
      mFrameSelection->HandleDrag(viewFrame, mPoint);

      mSelection->DoAutoScrollView(mPresContext,
                                   viewFrame.IsAlive() ? captureView : nsnull,
                                   mPoint, PR_TRUE);
    }
    return NS_OK;
  }
private:
  nsFrameSelection *mFrameSelection;
  nsTypedSelection *mSelection;
  nsPresContext *mPresContext;
  nsPoint mPoint;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIContent> mContent;
  PRUint32 mDelay;
};

NS_IMPL_ISUPPORTS1(nsAutoScrollTimer, nsITimerCallback)

nsresult NS_NewSelection(nsFrameSelection **aFrameSelection)
{
  nsFrameSelection *rlist = new nsFrameSelection;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  *aFrameSelection = rlist;
  NS_ADDREF(rlist);
  return NS_OK;
}

nsresult NS_NewDomSelection(nsISelection **aDomSelection)
{
  nsTypedSelection *rlist = new nsTypedSelection;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  *aDomSelection = (nsISelection *)rlist;
  NS_ADDREF(rlist);
  return NS_OK;
}

static PRInt8
GetIndexFromSelectionType(SelectionType aType)
{
    switch (aType)
    {
    case nsISelectionController::SELECTION_NORMAL: return 0; break;
    case nsISelectionController::SELECTION_SPELLCHECK: return 1; break;
    case nsISelectionController::SELECTION_IME_RAWINPUT: return 2; break;
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT: return 3; break;
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT: return 4; break;
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT: return 5; break;
    case nsISelectionController::SELECTION_ACCESSIBILITY: return 6; break;
    case nsISelectionController::SELECTION_FIND: return 7; break;
    default:
      return -1; break;
    }
    /* NOTREACHED */
    return 0;
}

static SelectionType 
GetSelectionTypeFromIndex(PRInt8 aIndex)
{
  switch (aIndex)
  {
    case 0: return nsISelectionController::SELECTION_NORMAL; break;
    case 1: return nsISelectionController::SELECTION_SPELLCHECK; break;
    case 2: return nsISelectionController::SELECTION_IME_RAWINPUT; break;
    case 3: return nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT; break;
    case 4: return nsISelectionController::SELECTION_IME_CONVERTEDTEXT; break;
    case 5: return nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT; break;
    case 6: return nsISelectionController::SELECTION_ACCESSIBILITY; break;
    case 7: return nsISelectionController::SELECTION_FIND; break;
    default:
      return nsISelectionController::SELECTION_NORMAL; break;
  }
  /* NOTREACHED */
  return 0;
}

/*
The limiter is used specifically for the text areas and textfields
In that case it is the DIV tag that is anonymously created for the text
areas/fields.  Text nodes and BR nodes fall beneath it.  In the case of a 
BR node the limiter will be the parent and the offset will point before or
after the BR node.  In the case of the text node the parent content is 
the text node itself and the offset will be the exact character position.
The offset is not important to check for validity.  Simply look at the 
passed in content.  If it equals the limiter then the selection point is valid.
If its parent it the limiter then the point is also valid.  In the case of 
NO limiter all points are valid since you are in a topmost iframe. (browser
or composer)
*/
PRBool       
IsValidSelectionPoint(nsFrameSelection *aFrameSel, nsINode *aNode)
{
  if (!aFrameSel || !aNode)
    return PR_FALSE;

  nsIContent *limiter = aFrameSel->GetLimiter();
  if (limiter && limiter != aNode && limiter != aNode->GetParent()) {
    //if newfocus == the limiter. that's ok. but if not there and not parent bad
    return PR_FALSE; //not in the right content. tLimiter said so
  }

  limiter = aFrameSel->GetAncestorLimiter();
  return !limiter || nsContentUtils::ContentIsDescendantOf(aNode, limiter);
}


NS_IMPL_ADDREF(nsSelectionIterator)
NS_IMPL_RELEASE(nsSelectionIterator)

NS_INTERFACE_MAP_BEGIN(nsSelectionIterator)
  NS_INTERFACE_MAP_ENTRY(nsIEnumerator)
  NS_INTERFACE_MAP_ENTRY(nsIBidirectionalEnumerator)
NS_INTERFACE_MAP_END_AGGREGATED(mDomSelection)


///////////BEGIN nsSelectionIterator methods

nsSelectionIterator::nsSelectionIterator(nsTypedSelection *aList)
:mIndex(0)
{
  if (!aList)
  {
    NS_NOTREACHED("nsFrameSelection");
    return;
  }
  mDomSelection = aList;
}



nsSelectionIterator::~nsSelectionIterator()
{
}



////////////END nsSelectionIterator methods

////////////BEGIN nsSelectionIterator methods



NS_IMETHODIMP
nsSelectionIterator::Next()
{
  mIndex++;
  PRInt32 cnt = mDomSelection->mRanges.Length();
  if (mIndex < cnt)
    return NS_OK;
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsSelectionIterator::Prev()
{
  mIndex--;
  if (mIndex >= 0 )
    return NS_OK;
  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsSelectionIterator::First()
{
  if (!mDomSelection)
    return NS_ERROR_NULL_POINTER;
  mIndex = 0;
  return NS_OK;
}



NS_IMETHODIMP
nsSelectionIterator::Last()
{
  if (!mDomSelection)
    return NS_ERROR_NULL_POINTER;
  mIndex = mDomSelection->mRanges.Length() - 1;
  return NS_OK;
}



NS_IMETHODIMP 
nsSelectionIterator::CurrentItem(nsISupports **aItem)
{
  *aItem = CurrentItem();
  if (!*aItem) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aItem);
  return NS_OK;
}

nsIRange*
nsSelectionIterator::CurrentItem()
{
  return mDomSelection->mRanges.SafeElementAt(mIndex, sEmptyData).mRange;
}



NS_IMETHODIMP
nsSelectionIterator::IsDone()
{
  PRInt32 cnt = mDomSelection->mRanges.Length();
  if (mIndex >= 0 && mIndex < cnt) {
    return NS_ENUMERATOR_FALSE;
  }
  return NS_OK;
}


////////////END nsSelectionIterator methods


////////////BEGIN nsFrameSelection methods

nsFrameSelection::nsFrameSelection()
  : mScrollableViewProvider(nsnull),
    mDelayedMouseEvent(PR_FALSE, 0, nsnull, nsMouseEvent::eReal)
{
  PRInt32 i;
  for (i = 0;i<nsISelectionController::NUM_SELECTIONTYPES;i++){
    mDomSelections[i] = new nsTypedSelection(this);
    if (!mDomSelections[i])
      break;
    mDomSelections[i]->SetType(GetSelectionTypeFromIndex(i));
  }
  mBatching = 0;
  mChangesDuringBatching = PR_FALSE;
  mNotifyFrames = PR_TRUE;
  mLimiter = nsnull; //no default limiter.
  mAncestorLimiter = nsnull;
  
  mMouseDoubleDownState = PR_FALSE;
  
  mHint = HINTLEFT;
#ifdef IBMBIDI
  mCaretBidiLevel = BIDI_LEVEL_UNDEFINED;
#endif
  mDragSelectingCells = PR_FALSE;
  mSelectingTableCellMode = 0;
  mSelectedCellIndex = 0;

  // Check to see if the autocopy pref is enabled
  //   and add the autocopy listener if it is
  if (nsContentUtils::GetBoolPref("clipboard.autocopy")) {
    nsAutoCopyListener *autoCopy = nsAutoCopyListener::GetInstance();

    if (autoCopy) {
      PRInt8 index =
        GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
      if (mDomSelections[index]) {
        autoCopy->Listen(mDomSelections[index]);
      }
    }
  }

  mDisplaySelection = nsISelectionController::SELECTION_OFF;

  mDelayedMouseEventValid = PR_FALSE;
  mSelectionChangeReason = nsISelectionListener::NO_REASON;
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameSelection)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameSelection)
  PRInt32 i;
  for (i = 0; i < nsISelectionController::NUM_SELECTIONTYPES; ++i) {
    tmp->mDomSelections[i] = nsnull;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCellParent)
  tmp->mSelectingTableCellMode = 0;
  tmp->mDragSelectingCells = PR_FALSE;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEndSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAppendStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mUnselectCellOnMouseUp)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMaintainRange)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameSelection)
  PRInt32 i;
  for (i = 0; i < nsISelectionController::NUM_SELECTIONTYPES; ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mDomSelections[i],
                                                         nsISelection)
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCellParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEndSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAppendStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mUnselectCellOnMouseUp)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMaintainRange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameSelection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameSelection)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameSelection)
  NS_INTERFACE_MAP_ENTRY(nsFrameSelection)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


nsresult
nsFrameSelection::FetchDesiredX(nscoord &aDesiredX) //the x position requested by the Key Handling for up down
{
  if (!mShell)
  {
    NS_ASSERTION(0,"fetch desired X failed\n");
    return NS_ERROR_FAILURE;
  }
  if (mDesiredXSet)
  {
    aDesiredX = mDesiredX;
    return NS_OK;
  }

  nsRefPtr<nsCaret> caret;
  nsresult result = mShell->GetCaret(getter_AddRefs(caret));
  if (NS_FAILED(result))
    return result;
  if (!caret)
    return NS_ERROR_NULL_POINTER;

  nsRect coord;
  PRBool  collapsed;
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  result = caret->SetCaretDOMSelection(mDomSelections[index]);
  if (NS_FAILED(result))
    return result;

  result = caret->GetCaretCoordinates(nsCaret::eClosestViewCoordinates, mDomSelections[index], &coord, &collapsed, nsnull);
  if (NS_FAILED(result))
    return result;
   
  aDesiredX = coord.x;
  return NS_OK;
}



void
nsFrameSelection::InvalidateDesiredX() //do not listen to mDesiredX you must get another.
{
  mDesiredXSet = PR_FALSE;
}



void
nsFrameSelection::SetDesiredX(nscoord aX) //set the mDesiredX
{
  mDesiredX = aX;
  mDesiredXSet = PR_TRUE;
}

nsresult
nsFrameSelection::GetRootForContentSubtree(nsIContent  *aContent,
                                           nsIContent **aParent)
{
  // This method returns the root of the sub-tree containing aContent.
  // We do this by searching up through the parent hierarchy, and stopping
  // when there are no more parents, or we hit a situation where the
  // parent/child relationship becomes invalid.
  //
  // An example of an invalid parent/child relationship is anonymous content.
  // Anonymous content has a pointer to its parent, but it is not listed
  // as a child of its parent. In this case, the anonymous content would
  // be considered the root of the subtree.

  if (!aContent || !aParent)
    return NS_ERROR_NULL_POINTER;

  *aParent = 0;

  nsIContent* child = aContent;

  while (child)
  {
    nsIContent* parent = child->GetParent();

    if (!parent)
      break;

    PRUint32 childCount = parent->GetChildCount();

    if (childCount < 1)
      break;

    PRInt32 childIndex = parent->IndexOf(child);

    if (childIndex < 0 || ((PRUint32)childIndex) >= childCount)
      break;

    child = parent;
  }

  NS_IF_ADDREF(*aParent = child);

  return NS_OK;
}

nsresult
nsFrameSelection::ConstrainFrameAndPointToAnchorSubtree(nsIFrame  *aFrame,
                                                        nsPoint&   aPoint,
                                                        nsIFrame **aRetFrame,
                                                        nsPoint&   aRetPoint)
{
  //
  // The whole point of this method is to return a frame and point that
  // that lie within the same valid subtree as the anchor node's frame,
  // for use with the method GetContentAndOffsetsFromPoint().
  //
  // A valid subtree is defined to be one where all the content nodes in
  // the tree have a valid parent-child relationship.
  //
  // If the anchor frame and aFrame are in the same subtree, aFrame will
  // be returned in aRetFrame. If they are in different subtrees, we
  // return the frame for the root of the subtree.
  //

  if (!aFrame || !aRetFrame)
    return NS_ERROR_NULL_POINTER;

  *aRetFrame = aFrame;
  aRetPoint  = aPoint;

  //
  // Get the frame and content for the selection's anchor point!
  //

  nsresult result;
  nsCOMPtr<nsIDOMNode> anchorNode;
  PRInt32 anchorOffset = 0;
  PRInt32 anchorFrameOffset = 0;

  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (! mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  result = mDomSelections[index]->GetAnchorNode(getter_AddRefs(anchorNode));

  if (NS_FAILED(result))
    return result;

  if (!anchorNode)
    return NS_OK;

  result = mDomSelections[index]->GetAnchorOffset(&anchorOffset);

  if (NS_FAILED(result))
    return result;

  nsIFrame *anchorFrame = 0;
  nsCOMPtr<nsIContent> anchorContent = do_QueryInterface(anchorNode);

  if (!anchorContent)
    return NS_ERROR_FAILURE;
  
  anchorFrame = GetFrameForNodeOffset(anchorContent, anchorOffset,
                                      mHint, &anchorFrameOffset);

  //
  // Now find the root of the subtree containing the anchor's content.
  //

  nsCOMPtr<nsIContent> anchorRoot;
  result = GetRootForContentSubtree(anchorContent, getter_AddRefs(anchorRoot));

  if (NS_FAILED(result))
    return result;

  //
  // Now find the root of the subtree containing aFrame's content.
  //

  nsIContent* content = aFrame->GetContent();

  if (content)
  {
    nsCOMPtr<nsIContent> contentRoot;

    result = GetRootForContentSubtree(content, getter_AddRefs(contentRoot));

    if (anchorRoot == contentRoot)
    {
      //
      // The anchor and AFrame's root are the same. There
      // is no need to constrain, simply return aFrame.
      //
      *aRetFrame = aFrame;
      return NS_OK;
    }
  }

  //
  // aFrame's root does not match the anchor's root, or there is no
  // content associated with aFrame. Just return the primary frame
  // for the anchor's root. We'll let GetContentAndOffsetsFromPoint()
  // find the closest frame aPoint.
  //

  NS_ENSURE_STATE(mShell);
  *aRetFrame = mShell->GetPrimaryFrameFor(anchorRoot);

  if (! *aRetFrame)
    return NS_ERROR_FAILURE;

  //
  // Now make sure that aRetPoint is converted to the same coordinate
  // system used by aRetFrame.
  //

  aRetPoint = aPoint + aFrame->GetOffsetTo(*aRetFrame);

  return NS_OK;
}

#ifdef IBMBIDI
void
nsFrameSelection::SetCaretBidiLevel(PRUint8 aLevel)
{
  // If the current level is undefined, we have just inserted new text.
  // In this case, we don't want to reset the keyboard language
  PRBool afterInsert = !!(mCaretBidiLevel & BIDI_LEVEL_UNDEFINED);
  mCaretBidiLevel = aLevel;
  
  nsIBidiKeyboard* bidiKeyboard = nsContentUtils::GetBidiKeyboard();
  if (bidiKeyboard && !afterInsert)
    bidiKeyboard->SetLangFromBidiLevel(aLevel);
  return;
}

PRUint8
nsFrameSelection::GetCaretBidiLevel() const
{
  return mCaretBidiLevel;
}

void
nsFrameSelection::UndefineCaretBidiLevel()
{
  mCaretBidiLevel |= BIDI_LEVEL_UNDEFINED;
}
#endif


#ifdef PRINT_RANGE
void printRange(nsIRange *aDomRange)
{
  if (!aDomRange)
  {
    printf("NULL nsIDOMRange\n");
  }
  nsINode* startNode = aDomRange->GetStartParent();
  nsINode* endNode = aDomRange->GetEndParent();
  PRInt32 startOffset = aDomRange->StartOffset();
  PRInt32 endOffset = aDomRange->EndOffset();
  
  printf("range: 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld\n",
         (unsigned long)aDomRange,
         (unsigned long)startNode, (long)startOffset,
         (unsigned long)endNode, (long)endOffset);
         
}
#endif /* PRINT_RANGE */

static
nsIAtom *GetTag(nsINode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (!content) 
  {
    NS_NOTREACHED("bad node passed to GetTag()");
    return nsnull;
  }
  
  return content->Tag();
}

// Returns the parent
nsINode*
ParentOffset(nsINode *aNode, PRInt32 *aChildOffset)
{
  if (!aNode || !aChildOffset)
    return nsnull;

  nsIContent* parent = aNode->GetParent();
  if (parent)
  {
    *aChildOffset = parent->IndexOf(aNode);

    return parent;
  }

  return nsnull;
}

static nsINode*
GetCellParent(nsINode *aDomNode)
{
    if (!aDomNode)
      return nsnull;
    nsINode* current = aDomNode;
    // Start with current node and look for a table cell
    while (current)
    {
      nsIAtom* tag = GetTag(current);
      if (tag == nsGkAtoms::td || tag == nsGkAtoms::th)
        return current;
      current = current->GetParent();
    }
    return nsnull;
}


void
nsFrameSelection::Init(nsIPresShell *aShell, nsIContent *aLimiter)
{
  mShell = aShell;
  mMouseDownState = PR_FALSE;
  mDesiredXSet = PR_FALSE;
  mLimiter = aLimiter;
  mCaretMovementStyle = nsContentUtils::GetIntPref("bidi.edit.caret_movement_style", 2);
}

nsresult
nsFrameSelection::MoveCaret(PRUint32          aKeycode,
                            PRBool            aContinueSelection,
                            nsSelectionAmount aAmount)
{
  NS_ENSURE_STATE(mShell);
  // Flush out layout, since we need it to be up to date to do caret
  // positioning.
  mShell->FlushPendingNotifications(Flush_Layout);

  if (!mShell) {
    return NS_OK;
  }

  nsPresContext *context = mShell->GetPresContext();
  if (!context)
    return NS_ERROR_FAILURE;

  PRBool isCollapsed;
  nscoord desiredX = 0; //we must keep this around and revalidate it when its just UP/DOWN

  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  nsRefPtr<nsTypedSelection> sel = mDomSelections[index];
  if (!sel)
    return NS_ERROR_NULL_POINTER;

  nsresult result = sel->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(result))
    return result;
  if (aKeycode == nsIDOMKeyEvent::DOM_VK_UP ||
      aKeycode == nsIDOMKeyEvent::DOM_VK_DOWN)
  {
    result = FetchDesiredX(desiredX);
    if (NS_FAILED(result))
      return result;
    SetDesiredX(desiredX);
  }

  PRInt32 caretStyle = nsContentUtils::GetIntPref("layout.selection.caret_style", 0);
#ifdef XP_MACOSX
  if (caretStyle == 0) {
    caretStyle = 2; // put caret at the selection edge in the |aKeycode| direction
  }
#endif

  if (!isCollapsed && !aContinueSelection && caretStyle == 2) {
    switch (aKeycode){
      case nsIDOMKeyEvent::DOM_VK_LEFT  :
      case nsIDOMKeyEvent::DOM_VK_UP    :
        {
          const nsIRange* anchorFocusRange = sel->GetAnchorFocusRange();
          if (anchorFocusRange) {
            sel->Collapse(anchorFocusRange->GetStartParent(),
                          anchorFocusRange->StartOffset());
          }
          mHint = HINTRIGHT;
          sel->ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                              PR_FALSE, PR_FALSE);
          return NS_OK;
        }

      case nsIDOMKeyEvent::DOM_VK_RIGHT :
      case nsIDOMKeyEvent::DOM_VK_DOWN  :
        {
          const nsIRange* anchorFocusRange = sel->GetAnchorFocusRange();
          if (anchorFocusRange) {
            sel->Collapse(anchorFocusRange->GetEndParent(),
                          anchorFocusRange->EndOffset());
          }
          mHint = HINTLEFT;
          sel->ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                              PR_FALSE, PR_FALSE);
          return NS_OK;
        }
    }
  }

  PRBool visualMovement = 
    (aKeycode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE || 
     aKeycode == nsIDOMKeyEvent::DOM_VK_DELETE ||
     aKeycode == nsIDOMKeyEvent::DOM_VK_HOME || 
     aKeycode == nsIDOMKeyEvent::DOM_VK_END) ?
    PR_FALSE : // Delete operations and home/end are always logical
    mCaretMovementStyle == 1 || (mCaretMovementStyle == 2 && !aContinueSelection);

  nsIFrame *frame;
  PRInt32 offsetused = 0;
  result = sel->GetPrimaryFrameForFocusNode(&frame, &offsetused, visualMovement);

  if (NS_FAILED(result) || !frame)
    return result?result:NS_ERROR_FAILURE;

  nsPeekOffsetStruct pos;
  //set data using mLimiter to stop on scroll views.  If we have a limiter then we stop peeking
  //when we hit scrollable views.  If no limiter then just let it go ahead
  pos.SetData(aAmount, eDirPrevious, offsetused, desiredX, 
              PR_TRUE, mLimiter != nsnull, PR_TRUE, visualMovement);

  nsBidiLevel baseLevel = nsBidiPresUtils::GetFrameBaseLevel(frame);
  
  HINT tHint(mHint); //temporary variable so we dont set mHint until it is necessary
  switch (aKeycode){
    case nsIDOMKeyEvent::DOM_VK_RIGHT : 
        InvalidateDesiredX();
        pos.mDirection = (baseLevel & 1) ? eDirPrevious : eDirNext;
      break;
    case nsIDOMKeyEvent::DOM_VK_LEFT :
        InvalidateDesiredX();
        pos.mDirection = (baseLevel & 1) ? eDirNext : eDirPrevious;
      break;
    case nsIDOMKeyEvent::DOM_VK_DELETE :
        InvalidateDesiredX();
        pos.mDirection = eDirNext;
      break;
    case nsIDOMKeyEvent::DOM_VK_BACK_SPACE : 
        InvalidateDesiredX();
        pos.mDirection = eDirPrevious;
      break;
    case nsIDOMKeyEvent::DOM_VK_DOWN : 
        pos.mAmount = eSelectLine;
        pos.mDirection = eDirNext;
      break;
    case nsIDOMKeyEvent::DOM_VK_UP : 
        pos.mAmount = eSelectLine;
        pos.mDirection = eDirPrevious;
      break;
    case nsIDOMKeyEvent::DOM_VK_HOME :
        InvalidateDesiredX();
        pos.mAmount = eSelectBeginLine;
      break;
    case nsIDOMKeyEvent::DOM_VK_END :
        InvalidateDesiredX();
        pos.mAmount = eSelectEndLine;
      break;
  default :return NS_ERROR_FAILURE;
  }
  PostReason(nsISelectionListener::KEYPRESS_REASON);
  if (NS_SUCCEEDED(result = frame->PeekOffset(&pos)) && pos.mResultContent)
  {
    nsIFrame *theFrame;
    PRInt32 currentOffset, frameStart, frameEnd;

    if (aAmount == eSelectCharacter || aAmount == eSelectWord)
    {
      // For left/right, PeekOffset() sets pos.mResultFrame correctly, but does not set pos.mAttachForward,
      // so determine the hint here based on the result frame and offset:
      // If we're at the end of a text frame, set the hint to HINTLEFT to indicate that we
      // want the caret displayed at the end of this frame, not at the beginning of the next one.
      theFrame = pos.mResultFrame;
      theFrame->GetOffsets(frameStart, frameEnd);
      currentOffset = pos.mContentOffset;
      if (frameEnd == currentOffset && !(frameStart == 0 && frameEnd == 0))
        tHint = HINTLEFT;
      else
        tHint = HINTRIGHT;
    } else {
      // For up/down and home/end, pos.mResultFrame might not be set correctly, or not at all.
      // In these cases, get the frame based on the content and hint returned by PeekOffset().
      tHint = (HINT)pos.mAttachForward;
      theFrame = GetFrameForNodeOffset(pos.mResultContent, pos.mContentOffset,
                                       tHint, &currentOffset);
      if (!theFrame)
        return NS_ERROR_FAILURE;

      theFrame->GetOffsets(frameStart, frameEnd);
    }

    if (context->BidiEnabled())
    {
      switch (aKeycode) {
        case nsIDOMKeyEvent::DOM_VK_HOME:
        case nsIDOMKeyEvent::DOM_VK_END:
          // set the caret Bidi level to the paragraph embedding level
          SetCaretBidiLevel(NS_GET_BASE_LEVEL(theFrame));
          break;

        default:
          // If the current position is not a frame boundary, it's enough just to take the Bidi level of the current frame
          if ((pos.mContentOffset != frameStart && pos.mContentOffset != frameEnd)
              || (eSelectLine == aAmount))
          {
            SetCaretBidiLevel(NS_GET_EMBEDDING_LEVEL(theFrame));
          }
          else
            BidiLevelFromMove(mShell, pos.mResultContent, pos.mContentOffset, aKeycode, tHint);
      }
    }
    result = TakeFocus(pos.mResultContent, pos.mContentOffset, pos.mContentOffset,
                       tHint, aContinueSelection, PR_FALSE);
  } else if (aKeycode == nsIDOMKeyEvent::DOM_VK_RIGHT && !aContinueSelection) {
    // Collapse selection if PeekOffset failed, we either
    //  1. bumped into the BRFrame, bug 207623
    //  2. had select-all in a text input (DIV range), bug 352759.
    PRBool isBRFrame = frame->GetType() == nsGkAtoms::brFrame;
    sel->Collapse(sel->GetFocusNode(), sel->GetFocusOffset());
    // Note: 'frame' might be dead here.
    if (!isBRFrame) {
      mHint = HINTLEFT; // We're now at the end of the frame to the left.
    }
    result = NS_OK;
  }
  if (NS_SUCCEEDED(result))
  {
    result = mDomSelections[index]->
      ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                     PR_FALSE, PR_FALSE);
  }

  return result;
}

//END nsFrameSelection methods


//BEGIN nsFrameSelection methods

NS_IMETHODIMP
nsTypedSelection::ToString(PRUnichar **aReturn)
{
  return ToStringWithFormat("text/plain", 0, 0, aReturn);
}


NS_IMETHODIMP
nsTypedSelection::ToStringWithFormat(const char * aFormatType, PRUint32 aFlags, 
                                   PRInt32 aWrapCol, PRUnichar **aReturn)
{
  nsresult rv = NS_OK;
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  
  nsCAutoString formatType( NS_DOC_ENCODER_CONTRACTID_BASE );
  formatType.Append(aFormatType);
  nsCOMPtr<nsIDocumentEncoder> encoder =
           do_CreateInstance(formatType.get(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> shell;
  rv = GetPresShell(getter_AddRefs(shell));
  if (NS_FAILED(rv) || !shell) {
    return NS_ERROR_FAILURE;
  }

  nsIDocument *doc = shell->GetDocument();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  NS_ASSERTION(domDoc, "Need a document");

  // Flags should always include OutputSelectionOnly if we're coming from here:
  aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  nsAutoString readstring;
  readstring.AssignASCII(aFormatType);
  rv = encoder->Init(domDoc, readstring, aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  encoder->SetSelection(this);
  if (aWrapCol != 0)
    encoder->SetWrapColumn(aWrapCol);

  nsAutoString tmp;
  rv = encoder->EncodeToString(tmp);
  *aReturn = ToNewUnicode(tmp);//get the unicode pointer from it. this is temporary
  return rv;
}

NS_IMETHODIMP
nsTypedSelection::SetInterlinePosition(PRBool aHintRight)
{
  if (!mFrameSelection)
    return NS_ERROR_NOT_INITIALIZED; // Can't do selection
  nsFrameSelection::HINT hint;
  if (aHintRight)
    hint = nsFrameSelection::HINTRIGHT;
  else
    hint = nsFrameSelection::HINTLEFT;
  mFrameSelection->SetHint(hint);
  
  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::GetInterlinePosition(PRBool *aHintRight)
{
  if (!mFrameSelection)
    return NS_ERROR_NOT_INITIALIZED; // Can't do selection
  *aHintRight = (mFrameSelection->GetHint() == nsFrameSelection::HINTRIGHT);
  return NS_OK;
}

nsPrevNextBidiLevels
nsFrameSelection::GetPrevNextBidiLevels(nsIContent *aNode,
                                        PRUint32    aContentOffset,
                                        PRBool      aJumpLines) const
{
  return GetPrevNextBidiLevels(aNode, aContentOffset, mHint, aJumpLines);
}

nsPrevNextBidiLevels
nsFrameSelection::GetPrevNextBidiLevels(nsIContent *aNode,
                                        PRUint32    aContentOffset,
                                        HINT        aHint,
                                        PRBool      aJumpLines) const
{
  // Get the level of the frames on each side
  nsIFrame    *currentFrame;
  PRInt32     currentOffset;
  PRInt32     frameStart, frameEnd;
  nsDirection direction;
  
  nsPrevNextBidiLevels levels;
  levels.SetData(nsnull, nsnull, 0, 0);

  currentFrame = GetFrameForNodeOffset(aNode, aContentOffset,
                                       aHint, &currentOffset);
  if (!currentFrame)
    return levels;

  currentFrame->GetOffsets(frameStart, frameEnd);

  if (0 == frameStart && 0 == frameEnd)
    direction = eDirPrevious;
  else if (frameStart == currentOffset)
    direction = eDirPrevious;
  else if (frameEnd == currentOffset)
    direction = eDirNext;
  else {
    // we are neither at the beginning nor at the end of the frame, so we have no worries
    levels.SetData(currentFrame, currentFrame,
                   NS_GET_EMBEDDING_LEVEL(currentFrame),
                   NS_GET_EMBEDDING_LEVEL(currentFrame));
    return levels;
  }

  nsIFrame *newFrame;
  PRInt32 offset;
  PRBool jumpedLine;
  nsresult rv = currentFrame->GetFrameFromDirection(direction, PR_FALSE,
                                                    aJumpLines, PR_TRUE,
                                                    &newFrame, &offset, &jumpedLine);
  if (NS_FAILED(rv))
    newFrame = nsnull;

  PRUint8 baseLevel = NS_GET_BASE_LEVEL(currentFrame);
  PRUint8 currentLevel = NS_GET_EMBEDDING_LEVEL(currentFrame);
  PRUint8 newLevel = newFrame ? NS_GET_EMBEDDING_LEVEL(newFrame) : baseLevel;
  
  // If not jumping lines, disregard br frames, since they might be positioned incorrectly.
  // XXX This could be removed once bug 339786 is fixed.
  if (!aJumpLines) {
    if (currentFrame->GetType() == nsGkAtoms::brFrame) {
      currentFrame = nsnull;
      currentLevel = baseLevel;
    }
    if (newFrame && newFrame->GetType() == nsGkAtoms::brFrame) {
      newFrame = nsnull;
      newLevel = baseLevel;
    }
  }
  
  if (direction == eDirNext)
    levels.SetData(currentFrame, newFrame, currentLevel, newLevel);
  else
    levels.SetData(newFrame, currentFrame, newLevel, currentLevel);

  return levels;
}

nsresult
nsFrameSelection::GetFrameFromLevel(nsIFrame    *aFrameIn,
                                    nsDirection  aDirection,
                                    PRUint8      aBidiLevel,
                                    nsIFrame   **aFrameOut) const
{
  NS_ENSURE_STATE(mShell);
  PRUint8 foundLevel = 0;
  nsIFrame *foundFrame = aFrameIn;

  nsCOMPtr<nsIFrameEnumerator> frameTraversal;
  nsresult result;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,&result));
  if (NS_FAILED(result))
      return result;

  result = trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                                   mShell->GetPresContext(), aFrameIn,
                                   eLeaf,
                                   PR_FALSE, // aVisual
                                   PR_FALSE, // aLockInScrollView
                                   PR_FALSE  // aFollowOOFs
                                   );
  if (NS_FAILED(result))
    return result;

  do {
    *aFrameOut = foundFrame;
    if (aDirection == eDirNext)
      frameTraversal->Next();
    else 
      frameTraversal->Prev();

    foundFrame = frameTraversal->CurrentItem();
    if (!foundFrame)
      return NS_ERROR_FAILURE;
    foundLevel = NS_GET_EMBEDDING_LEVEL(foundFrame);

  } while (foundLevel > aBidiLevel);

  return NS_OK;
}


nsresult
nsFrameSelection::MaintainSelection(nsSelectionAmount aAmount)
{
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  mMaintainedAmount = aAmount;

  const nsIRange* anchorFocusRange =
    mDomSelections[index]->GetAnchorFocusRange();
  if (anchorFocusRange) {
    return anchorFocusRange->CloneRange(getter_AddRefs(mMaintainRange));
  }

  mMaintainRange = nsnull;
  return NS_OK;
}


/** After moving the caret, its Bidi level is set according to the following rules:
 *
 *  After moving over a character with left/right arrow, set to the Bidi level of the last moved over character.
 *  After Home and End, set to the paragraph embedding level.
 *  After up/down arrow, PageUp/Down, set to the lower level of the 2 surrounding characters.
 *  After mouse click, set to the level of the current frame.
 *
 *  The following two methods use GetPrevNextBidiLevels to determine the new Bidi level.
 *  BidiLevelFromMove is called when the caret is moved in response to a keyboard event
 *
 * @param aPresShell is the presentation shell
 * @param aNode is the content node
 * @param aContentOffset is the new caret position, as an offset into aNode
 * @param aKeycode is the keyboard event that moved the caret to the new position
 * @param aHint is the hint indicating in what logical direction the caret moved
 */
void nsFrameSelection::BidiLevelFromMove(nsIPresShell* aPresShell,
                                         nsIContent   *aNode,
                                         PRUint32      aContentOffset,
                                         PRUint32      aKeycode,
                                         HINT          aHint)
{
  switch (aKeycode) {

    // Right and Left: the new cursor Bidi level is the level of the character moved over
    case nsIDOMKeyEvent::DOM_VK_RIGHT:
    case nsIDOMKeyEvent::DOM_VK_LEFT:
    {
      nsPrevNextBidiLevels levels = GetPrevNextBidiLevels(aNode, aContentOffset,
                                                          aHint, PR_FALSE);

      if (HINTLEFT == aHint)
        SetCaretBidiLevel(levels.mLevelBefore);
      else
        SetCaretBidiLevel(levels.mLevelAfter);
      break;
    }
      /*
    // Up and Down: the new cursor Bidi level is the smaller of the two surrounding characters      
    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_DOWN:
      GetPrevNextBidiLevels(aContext, aNode, aContentOffset, &firstFrame, &secondFrame, &firstLevel, &secondLevel);
      aPresShell->SetCaretBidiLevel(PR_MIN(firstLevel, secondLevel));
      break;
      */

    default:
      UndefineCaretBidiLevel();
  }
}

/**
 * BidiLevelFromClick is called when the caret is repositioned by clicking the mouse
 *
 * @param aNode is the content node
 * @param aContentOffset is the new caret position, as an offset into aNode
 */
void nsFrameSelection::BidiLevelFromClick(nsIContent *aNode,
                                          PRUint32    aContentOffset)
{
  nsIFrame* clickInFrame=nsnull;
  PRInt32 OffsetNotUsed;

  clickInFrame = GetFrameForNodeOffset(aNode, aContentOffset, mHint, &OffsetNotUsed);
  if (!clickInFrame)
    return;

  SetCaretBidiLevel(NS_GET_EMBEDDING_LEVEL(clickInFrame));
}


PRBool
nsFrameSelection::AdjustForMaintainedSelection(nsIContent *aContent,
                                               PRInt32     aOffset)
{
  if (!mMaintainRange)
    return PR_FALSE;

  if (!aContent) {
    return PR_FALSE;
  }

  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return PR_FALSE;

  nsINode* rangeStartNode = mMaintainRange->GetStartParent();
  nsINode* rangeEndNode = mMaintainRange->GetEndParent();
  PRInt32 rangeStartOffset = mMaintainRange->StartOffset();
  PRInt32 rangeEndOffset = mMaintainRange->EndOffset();

  PRInt32 relToStart =
    nsContentUtils::ComparePoints(rangeStartNode, rangeStartOffset,
                                  aContent, aOffset);
  PRInt32 relToEnd =
    nsContentUtils::ComparePoints(rangeEndNode, rangeEndOffset,
                                  aContent, aOffset);

  // If aContent/aOffset is inside the maintained selection, or if it is on the
  // "anchor" side of the maintained selection, we need to do something.
  if (relToStart < 0 && relToEnd > 0 ||
      (relToStart > 0 &&
       mDomSelections[index]->GetDirection() == eDirNext) ||
      (relToEnd < 0 &&
       mDomSelections[index]->GetDirection() == eDirPrevious)) {
    // Set the current range to the maintained range. 
    mDomSelections[index]->ReplaceAnchorFocusRange(mMaintainRange);
    if (relToStart < 0 && relToEnd > 0) {
      // We're inside the maintained selection, just keep it selected.
      return PR_TRUE;
    }
    // Reverse the direction of the selection so that the anchor will be on the 
    // far side of the maintained selection, relative to aContent/aOffset.
    mDomSelections[index]->SetDirection(relToStart > 0 ? eDirPrevious : eDirNext);
  }
  return PR_FALSE;
}


nsresult
nsFrameSelection::HandleClick(nsIContent *aNewFocus,
                              PRUint32    aContentOffset,
                              PRUint32    aContentEndOffset,
                              PRBool      aContinueSelection, 
                              PRBool      aMultipleSelection,
                              PRBool      aHint) 
{
  if (!aNewFocus)
    return NS_ERROR_INVALID_ARG;

  InvalidateDesiredX();

  if (!aContinueSelection) {
    mMaintainRange = nsnull;
    if (!IsValidSelectionPoint(this, aNewFocus)) {
      mAncestorLimiter = nsnull;
    }
  }

  // Don't take focus when dragging off of a table
  if (!mDragSelectingCells)
  {
    BidiLevelFromClick(aNewFocus, aContentOffset);
    PostReason(nsISelectionListener::MOUSEDOWN_REASON + nsISelectionListener::DRAG_REASON);
    if (aContinueSelection &&
        AdjustForMaintainedSelection(aNewFocus, aContentOffset))
      return NS_OK; //shift clicked to maintained selection. rejected.

    return TakeFocus(aNewFocus, aContentOffset, aContentEndOffset, HINT(aHint),
                     aContinueSelection, aMultipleSelection);
  }
  
  return NS_OK;
}

void
nsFrameSelection::HandleDrag(nsIFrame *aFrame, nsPoint aPoint)
{
  if (!aFrame || !mShell)
    return;

  nsresult result;
  nsIFrame *newFrame = 0;
  nsPoint   newPoint;

  result = ConstrainFrameAndPointToAnchorSubtree(aFrame, aPoint, &newFrame, newPoint);
  if (NS_FAILED(result))
    return;
  if (!newFrame)
    return;

  nsIFrame::ContentOffsets offsets =
      newFrame->GetContentOffsetsFromPoint(newPoint);
  if (!offsets.content)
    return;

  if ((newFrame->GetStateBits() & NS_FRAME_SELECTED_CONTENT) &&
       AdjustForMaintainedSelection(offsets.content, offsets.offset))
    return;

  // Adjust offsets according to maintained amount
  if (mMaintainRange && 
      mMaintainedAmount != eSelectNoAmount) {    
    
    nsINode* rangenode = mMaintainRange->GetStartParent();
    PRInt32 rangeOffset = mMaintainRange->StartOffset();
    PRInt32 relativePosition =
      nsContentUtils::ComparePoints(rangenode, rangeOffset,
                                    offsets.content, offsets.offset);

    nsDirection direction = relativePosition > 0 ? eDirPrevious : eDirNext;
    nsSelectionAmount amount = mMaintainedAmount;
    if (amount == eSelectBeginLine && direction == eDirNext)
      amount = eSelectEndLine;

    PRInt32 offset;
    nsIFrame* frame = GetFrameForNodeOffset(offsets.content, offsets.offset, HINTRIGHT, &offset);

    if (frame && amount == eSelectWord && direction == eDirPrevious) {
      // To avoid selecting the previous word when at start of word,
      // first move one character forward.
      nsPeekOffsetStruct charPos;
      charPos.SetData(eSelectCharacter, eDirNext, offset, 0,
                      PR_FALSE, mLimiter != nsnull, PR_FALSE, PR_FALSE);
      if (NS_SUCCEEDED(frame->PeekOffset(&charPos))) {
        frame = charPos.mResultFrame;
        offset = charPos.mContentOffset;
      }
    }

    nsPeekOffsetStruct pos;
    pos.SetData(amount, direction, offset, 0,
                PR_FALSE, mLimiter != nsnull, PR_FALSE, PR_FALSE);

    if (frame && NS_SUCCEEDED(frame->PeekOffset(&pos)) && pos.mResultContent) {
      offsets.content = pos.mResultContent;
      offsets.offset = pos.mContentOffset;
    }
  }
  
  HandleClick(offsets.content, offsets.offset, offsets.offset,
              PR_TRUE, PR_FALSE, offsets.associateWithNext);
}

nsresult
nsFrameSelection::StartAutoScrollTimer(nsIView  *aView,
                                       nsPoint   aPoint,
                                       PRUint32  aDelay)
{
  NS_ENSURE_STATE(mShell);
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[index]->StartAutoScrollTimer(mShell->GetPresContext(),
                                                     aView, aPoint, aDelay);
}

void
nsFrameSelection::StopAutoScrollTimer()
{
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return;

  mDomSelections[index]->StopAutoScrollTimer();
}

/**
hard to go from nodes to frames, easy the other way!
 */
nsresult
nsFrameSelection::TakeFocus(nsIContent *aNewFocus,
                            PRUint32    aContentOffset,
                            PRUint32    aContentEndOffset,
                            HINT        aHint,
                            PRBool      aContinueSelection,
                            PRBool      aMultipleSelection)
{
  if (!aNewFocus)
    return NS_ERROR_NULL_POINTER;

  NS_ENSURE_STATE(mShell);

  if (!IsValidSelectionPoint(this,aNewFocus))
    return NS_ERROR_FAILURE;

  // Clear all table selection data
  mSelectingTableCellMode = 0;
  mDragSelectingCells = PR_FALSE;
  mStartSelectedCell = nsnull;
  mEndSelectedCell = nsnull;
  mAppendStartSelectedCell = nsnull;

  //HACKHACKHACK
  if (!aNewFocus->GetParent())
    return NS_ERROR_FAILURE;
  //END HACKHACKHACK /checking for root frames/content

  mHint = aHint;
  
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  //traverse through document and unselect crap here
  if (!aContinueSelection) {//single click? setting cursor down
    PRUint32 batching = mBatching;//hack to use the collapse code.
    PRBool changes = mChangesDuringBatching;
    mBatching = 1;

    if (aMultipleSelection) {
      // Remove existing collapsed ranges as there's no point in having 
      // non-anchor/focus collapsed ranges.
      mDomSelections[index]->RemoveCollapsedRanges();

      nsCOMPtr<nsIRange> newRange = new nsRange();
      if (!newRange) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      newRange->SetStart(aNewFocus, aContentOffset);
      newRange->SetEnd(aNewFocus, aContentOffset);
      mDomSelections[index]->AddRange(newRange);
      mBatching = batching;
      mChangesDuringBatching = changes;
    }
    else
    {
      PRBool oldDesiredXSet = mDesiredXSet; //need to keep old desired X if it was set.
      mDomSelections[index]->Collapse(aNewFocus, aContentOffset);
      mDesiredXSet = oldDesiredXSet; //now reset desired X back.
      mBatching = batching;
      mChangesDuringBatching = changes;
    }
    if (aContentEndOffset != aContentOffset)
      mDomSelections[index]->Extend(aNewFocus, aContentEndOffset);

    //find out if we are inside a table. if so, find out which one and which cell
    //once we do that, the next time we get a takefocus, check the parent tree. 
    //if we are no longer inside same table ,cell then switch to table selection mode.
    // BUT only do this in an editor

    NS_ENSURE_STATE(mShell);
    PRInt16 displaySelection;
    nsresult result = mShell->GetSelectionFlags(&displaySelection);
    if (NS_FAILED(result))
      return result;

    // Editor has DISPLAY_ALL selection type
    if (displaySelection == nsISelectionDisplay::DISPLAY_ALL)
    {
      mCellParent = GetCellParent(aNewFocus);
#ifdef DEBUG_TABLE_SELECTION
      if (mCellParent)
        printf(" * TakeFocus - Collapsing into new cell\n");
#endif
    }
  }
  else {
    // Now update the range list:
    if (aContinueSelection && aNewFocus)
    {
      PRInt32 offset;
      nsINode *cellparent = GetCellParent(aNewFocus);
      if (mCellParent && cellparent && cellparent != mCellParent) //switch to cell selection mode
      {
#ifdef DEBUG_TABLE_SELECTION
printf(" * TakeFocus - moving into new cell\n");
#endif
        nsMouseEvent event(PR_FALSE, 0, nsnull, nsMouseEvent::eReal);

        // Start selecting in the cell we were in before
        nsINode* parent = ParentOffset(mCellParent, &offset);
        if (parent)
          HandleTableSelection(parent, offset,
                               nsISelectionPrivate::TABLESELECTION_CELL, &event);

        // Find the parent of this new cell and extend selection to it
        parent = ParentOffset(cellparent, &offset);

        // XXXX We need to REALLY get the current key shift state
        //  (we'd need to add event listener -- let's not bother for now)
        event.isShift = PR_FALSE; //aContinueSelection;
        if (parent)
        {
          mCellParent = cellparent;
          // Continue selection into next cell
          HandleTableSelection(parent, offset,
                               nsISelectionPrivate::TABLESELECTION_CELL, &event);
        }
      }
      else
      {
        // XXXX Problem: Shift+click in browser is appending text selection to selected table!!!
        //   is this the place to erase seleced cells ?????
        if (mDomSelections[index]->GetDirection() == eDirNext && aContentEndOffset > aContentOffset) //didn't go far enough 
        {
          mDomSelections[index]->Extend(aNewFocus, aContentEndOffset);//this will only redraw the diff 
        }
        else
          mDomSelections[index]->Extend(aNewFocus, aContentOffset);
      }
    }
  }

  // Don't notify selection listeners if batching is on:
  if (GetBatching())
    return NS_OK;
  return NotifySelectionListeners(nsISelectionController::SELECTION_NORMAL);
}



SelectionDetails*
nsFrameSelection::LookUpSelection(nsIContent *aContent,
                                  PRInt32 aContentOffset,
                                  PRInt32 aContentLength,
                                  PRBool aSlowCheck) const
{
  if (!aContent || !mShell)
    return nsnull;

  SelectionDetails* details = nsnull;

  for (PRInt32 j = 0; j < nsISelectionController::NUM_SELECTIONTYPES; j++) {
    if (mDomSelections[j]) {
      mDomSelections[j]->LookUpSelection(aContent, aContentOffset,
          aContentLength, &details, (SelectionType)(1<<j), aSlowCheck);
    }
  }

  return details;
}

void
nsFrameSelection::SetMouseDownState(PRBool aState)
{
  if (mMouseDownState == aState)
    return;

  mMouseDownState = aState;
    
  if (!mMouseDownState)
  {
    mDragSelectingCells = PR_FALSE;
    PostReason(nsISelectionListener::MOUSEUP_REASON);
    NotifySelectionListeners(nsISelectionController::SELECTION_NORMAL); //notify that reason is mouse up please.
  }
}

nsISelection*
nsFrameSelection::GetSelection(SelectionType aType) const
{
  PRInt8 index = GetIndexFromSelectionType(aType);
  if (index < 0)
    return nsnull;

  return static_cast<nsISelection*>(mDomSelections[index]);
}

nsresult
nsFrameSelection::ScrollSelectionIntoView(SelectionType   aType,
                                          SelectionRegion aRegion,
                                          PRBool          aIsSynchronous) const
{
  PRInt8 index = GetIndexFromSelectionType(aType);
  if (index < 0)
    return NS_ERROR_INVALID_ARG;

  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return mDomSelections[index]->ScrollIntoView(aRegion, aIsSynchronous,
                                               PR_TRUE);
}

nsresult
nsFrameSelection::RepaintSelection(SelectionType aType) const
{
  PRInt8 index = GetIndexFromSelectionType(aType);
  if (index < 0)
    return NS_ERROR_INVALID_ARG;
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;
  NS_ENSURE_STATE(mShell);
  return mDomSelections[index]->Repaint(mShell->GetPresContext());
}
 
nsIFrame*
nsFrameSelection::GetFrameForNodeOffset(nsIContent *aNode,
                                        PRInt32     aOffset,
                                        HINT        aHint,
                                        PRInt32    *aReturnOffset) const
{
  if (!aNode || !aReturnOffset || !mShell)
    return nsnull;

  if (aOffset < 0)
    return nsnull;

  *aReturnOffset = aOffset;

  nsCOMPtr<nsIContent> theNode = aNode;

  if (aNode->IsNodeOfType(nsINode::eELEMENT))
  {
    PRInt32 childIndex  = 0;
    PRInt32 numChildren = theNode->GetChildCount();

    if (aHint == HINTLEFT)
    {
      if (aOffset > 0)
        childIndex = aOffset - 1;
      else
        childIndex = aOffset;
    }
    else // HINTRIGHT
    {
      if (aOffset >= numChildren)
      {
        if (numChildren > 0)
          childIndex = numChildren - 1;
        else
          childIndex = 0;
      }
      else
        childIndex = aOffset;
    }
    
    if (childIndex > 0 || numChildren > 0) {
      nsCOMPtr<nsIContent> childNode = theNode->GetChildAt(childIndex);

      if (!childNode)
        return nsnull;

      theNode = childNode;
    }

#ifdef DONT_DO_THIS_YET
    // XXX: We can't use this code yet because the hinting
    //      can cause us to attatch to the wrong line frame.

    // Now that we have the child node, check if it too
    // can contain children. If so, call this method again!

    if (theNode->IsNodeOfType(nsINode::eELEMENT))
    {
      PRInt32 newOffset = 0;

      if (aOffset > childIndex)
      {
        numChildren = theNode->GetChildCount();

        newOffset = numChildren;
      }

      return GetFrameForNodeOffset(theNode, newOffset, aHint, aReturnOffset);
    }
    else
#endif // DONT_DO_THIS_YET
    {
      // Check to see if theNode is a text node. If it is, translate
      // aOffset into an offset into the text node.

      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(theNode);

      if (textNode)
      {
        if (aOffset > childIndex)
        {
          PRUint32 textLength = 0;

          nsresult rv = textNode->GetLength(&textLength);
          if (NS_FAILED(rv))
            return nsnull;

          *aReturnOffset = (PRInt32)textLength;
        }
        else
          *aReturnOffset = 0;
      }
    }
  }
  
  nsIFrame* returnFrame = mShell->GetPrimaryFrameFor(theNode);
  if (!returnFrame)
    return nsnull;

  // find the child frame containing the offset we want
  returnFrame->GetChildFrameContainingOffset(*aReturnOffset, aHint,
                                             &aOffset, &returnFrame);
  return returnFrame;
}

void
nsFrameSelection::CommonPageMove(PRBool aForward,
                                 PRBool aExtend,
                                 nsIScrollableView *aScrollableView)
{
  if (!aScrollableView)
    return;
  // expected behavior for PageMove is to scroll AND move the caret
  // and remain relative position of the caret in view. see Bug 4302.

  nsresult result;
  //get the frame from the scrollable view

  nsIFrame* mainframe = nsnull;

  // The view's client data points back to its frame
  nsIView *scrolledView;
  result = aScrollableView->GetScrolledView(scrolledView);

  if (NS_FAILED(result))
    return;

  if (scrolledView)
    mainframe = static_cast<nsIFrame*>(scrolledView->GetClientData());

  if (!mainframe)
    return;

  // find out where the caret is.
  // we should know mDesiredX value of nsFrameSelection, but I havent seen that behavior in other windows applications yet.
  nsISelection* domSel = GetSelection(nsISelectionController::SELECTION_NORMAL);
  
  if (!domSel) 
    return;
  
  nsRefPtr<nsCaret> caret;
  nsRect caretPos;
  PRBool isCollapsed;
  result = mShell->GetCaret(getter_AddRefs(caret));
  
  if (NS_FAILED(result)) 
    return;
  
  nsIView *caretView;
  result = caret->GetCaretCoordinates(nsCaret::eClosestViewCoordinates, domSel, &caretPos, &isCollapsed, &caretView);
  
  if (NS_FAILED(result)) 
    return;
  
  //need to adjust caret jump by percentage scroll
  nsSize scrollDelta;
  aScrollableView->GetPageScrollDistances(&scrollDelta);

  if (aForward)
    caretPos.y += scrollDelta.height;
  else
    caretPos.y -= scrollDelta.height;

  
  if (caretView)
  {
    caretPos += caretView->GetOffsetTo(scrolledView);
  }
    
  // get a content at desired location
  nsPoint desiredPoint;
  desiredPoint.x = caretPos.x;
  desiredPoint.y = caretPos.y + caretPos.height/2;
  nsIFrame::ContentOffsets offsets =
      mainframe->GetContentOffsetsFromPoint(desiredPoint);

  if (!offsets.content)
    return;

  // scroll one page
  aScrollableView->ScrollByPages(0, aForward ? 1 : -1);

  // place the caret
  HandleClick(offsets.content, offsets.offset,
              offsets.offset, aExtend, PR_FALSE, PR_TRUE);
}

nsresult
nsFrameSelection::CharacterMove(PRBool aForward, PRBool aExtend)
{
  if (aForward)
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_RIGHT,aExtend,eSelectCharacter);
  else
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_LEFT,aExtend,eSelectCharacter);
}

nsresult
nsFrameSelection::CharacterExtendForDelete()
{
  return MoveCaret(nsIDOMKeyEvent::DOM_VK_DELETE, PR_TRUE, eSelectCharacter);
}

nsresult
nsFrameSelection::WordMove(PRBool aForward, PRBool aExtend)
{
  if (aForward)
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_RIGHT,aExtend,eSelectWord);
  else
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_LEFT,aExtend,eSelectWord);
}

nsresult
nsFrameSelection::WordExtendForDelete(PRBool aForward)
{
  if (aForward)
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_DELETE, PR_TRUE, eSelectWord);
  else
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_BACK_SPACE, PR_TRUE, eSelectWord);
}

nsresult
nsFrameSelection::LineMove(PRBool aForward, PRBool aExtend)
{
  if (aForward)
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_DOWN,aExtend,eSelectLine);
  else
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_UP,aExtend,eSelectLine);
}

nsresult
nsFrameSelection::IntraLineMove(PRBool aForward, PRBool aExtend)
{
  if (aForward)
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_END,aExtend,eSelectLine);
  else
    return MoveCaret(nsIDOMKeyEvent::DOM_VK_HOME,aExtend,eSelectLine);
}

nsresult
nsFrameSelection::SelectAll()
{
  nsCOMPtr<nsIContent> rootContent;
  if (mLimiter)
  {
    rootContent = mLimiter;//addrefit
  }
  else if (mAncestorLimiter) {
    rootContent = mAncestorLimiter;
  }
  else
  {
    NS_ENSURE_STATE(mShell);
    nsIDocument *doc = mShell->GetDocument();
    if (!doc)
      return NS_ERROR_FAILURE;
    rootContent = doc->GetRootContent();
    if (!rootContent)
      return NS_ERROR_FAILURE;
  }
  PRInt32 numChildren = rootContent->GetChildCount();
  PostReason(nsISelectionListener::NO_REASON);
  return TakeFocus(rootContent, 0, numChildren, HINTLEFT, PR_FALSE, PR_FALSE);
}

//////////END FRAMESELECTION

void
nsFrameSelection::StartBatchChanges()
{
  mBatching++;
}

void
nsFrameSelection::EndBatchChanges()
{
  mBatching--;
  NS_ASSERTION(mBatching >=0,"Bad mBatching");
  if (mBatching == 0 && mChangesDuringBatching){
    mChangesDuringBatching = PR_FALSE;
    NotifySelectionListeners(nsISelectionController::SELECTION_NORMAL);
  }
}


nsresult
nsFrameSelection::NotifySelectionListeners(SelectionType aType)
{
  PRInt8 index = GetIndexFromSelectionType(aType);
  if (index >=0 && mDomSelections[index])
  {
    return mDomSelections[index]->NotifySelectionListeners();
  }
  return NS_ERROR_FAILURE;
}

// Start of Table Selection methods

static PRBool IsCell(nsIContent *aContent)
{
  return ((aContent->Tag() == nsGkAtoms::td ||
           aContent->Tag() == nsGkAtoms::th) &&
          aContent->IsNodeOfType(nsINode::eHTML));
}

nsITableCellLayout* 
nsFrameSelection::GetCellLayout(nsIContent *aCellContent) const
{
  NS_ENSURE_TRUE(mShell, nsnull);
  // Get frame for cell
  nsIFrame *cellFrame = mShell->GetPrimaryFrameFor(aCellContent);
  if (!cellFrame)
    return nsnull;

  nsITableCellLayout *cellLayoutObject = do_QueryFrame(cellFrame);
  return cellLayoutObject;
}

nsITableLayout* 
nsFrameSelection::GetTableLayout(nsIContent *aTableContent) const
{
  NS_ENSURE_TRUE(mShell, nsnull);
  // Get frame for table
  nsIFrame *tableFrame = mShell->GetPrimaryFrameFor(aTableContent);
  if (!tableFrame)
    return nsnull;

  nsITableLayout *tableLayoutObject = do_QueryFrame(tableFrame);
  return tableLayoutObject;
}

nsresult
nsFrameSelection::ClearNormalSelection()
{
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[index]->RemoveAllRanges();
}

static nsIContent*
GetFirstSelectedContent(nsIRange* aRange)
{
  if (!aRange) {
    return nsnull;
  }

  NS_PRECONDITION(aRange->GetStartParent(), "Must have start parent!");
  NS_PRECONDITION(aRange->GetStartParent()->IsNodeOfType(nsINode::eELEMENT),
                  "Unexpected parent");

  return aRange->GetStartParent()->GetChildAt(aRange->StartOffset());
}

// Table selection support.
// TODO: Separate table methods into a separate nsITableSelection interface
nsresult
nsFrameSelection::HandleTableSelection(nsINode *aParentContent,
                                       PRInt32 aContentOffset,
                                       PRInt32 aTarget,
                                       nsMouseEvent *aMouseEvent)
{
  NS_ENSURE_TRUE(aParentContent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aMouseEvent, NS_ERROR_NULL_POINTER);

  if (mMouseDownState && mDragSelectingCells && (aTarget & nsISelectionPrivate::TABLESELECTION_TABLE))
  {
    // We were selecting cells and user drags mouse in table border or inbetween cells,
    //  just do nothing
      return NS_OK;
  }

  nsresult result = NS_OK;

  nsIContent *childContent = aParentContent->GetChildAt(aContentOffset);

  // When doing table selection, always set the direction to next so
  // we can be sure that anchorNode's offset always points to the
  // selected cell
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  mDomSelections[index]->SetDirection(eDirNext);

  // Stack-class to wrap all table selection changes in 
  //  BeginBatchChanges() / EndBatchChanges()
  nsSelectionBatcher selectionBatcher(mDomSelections[index]);

  PRInt32 startRowIndex, startColIndex, curRowIndex, curColIndex;
  if (mMouseDownState && mDragSelectingCells)
  {
    // We are drag-selecting
    if (aTarget != nsISelectionPrivate::TABLESELECTION_TABLE)
    {
      // If dragging in the same cell as last event, do nothing
      if (mEndSelectedCell == childContent)
        return NS_OK;

#ifdef DEBUG_TABLE_SELECTION
printf(" mStartSelectedCell = %x, mEndSelectedCell = %x, childContent = %x \n", mStartSelectedCell, mEndSelectedCell, childContent);
#endif
      // aTarget can be any "cell mode",
      //  so we can easily drag-select rows and columns 
      // Once we are in row or column mode,
      //  we can drift into any cell to stay in that mode
      //  even if aTarget = TABLESELECTION_CELL

      if (mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_ROW ||
          mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_COLUMN)
      {
        if (mEndSelectedCell)
        {
          // Also check if cell is in same row/col
          result = GetCellIndexes(mEndSelectedCell, startRowIndex, startColIndex);
          if (NS_FAILED(result)) return result;
          result = GetCellIndexes(childContent, curRowIndex, curColIndex);
          if (NS_FAILED(result)) return result;
        
#ifdef DEBUG_TABLE_SELECTION
printf(" curRowIndex = %d, startRowIndex = %d, curColIndex = %d, startColIndex = %d\n", curRowIndex, startRowIndex, curColIndex, startColIndex);
#endif
          if ((mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_ROW && startRowIndex == curRowIndex) ||
              (mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_COLUMN && startColIndex == curColIndex)) 
            return NS_OK;
        }
#ifdef DEBUG_TABLE_SELECTION
printf(" Dragged into a new column or row\n");
#endif
        // Continue dragging row or column selection
        return SelectRowOrColumn(childContent, mSelectingTableCellMode);
      }
      else if (mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_CELL)
      {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Dragged into a new cell\n");
#endif
        // Trick for quick selection of rows and columns
        // Hold down shift, then start selecting in one direction
        // If next cell dragged into is in same row, select entire row,
        //   if next cell is in same column, select entire column
        if (mStartSelectedCell && aMouseEvent->isShift)
        {
          result = GetCellIndexes(mStartSelectedCell, startRowIndex, startColIndex);
          if (NS_FAILED(result)) return result;
          result = GetCellIndexes(childContent, curRowIndex, curColIndex);
          if (NS_FAILED(result)) return result;
          
          if (startRowIndex == curRowIndex || 
              startColIndex == curColIndex)
          {
            // Force new selection block
            mStartSelectedCell = nsnull;
            mDomSelections[index]->RemoveAllRanges();

            if (startRowIndex == curRowIndex)
              mSelectingTableCellMode = nsISelectionPrivate::TABLESELECTION_ROW;
            else
              mSelectingTableCellMode = nsISelectionPrivate::TABLESELECTION_COLUMN;

            return SelectRowOrColumn(childContent, mSelectingTableCellMode);
          }
        }
        
        // Reselect block of cells to new end location
        return SelectBlockOfCells(mStartSelectedCell, childContent);
      }
    }
    // Do nothing if dragging in table, but outside a cell
    return NS_OK;
  }
  else 
  {
    // Not dragging  -- mouse event is down or up
    if (mMouseDownState)
    {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Mouse down event\n");
#endif
      // Clear cell we stored in mouse-down
      mUnselectCellOnMouseUp = nsnull;
      
      if (aTarget == nsISelectionPrivate::TABLESELECTION_CELL)
      {
        PRBool isSelected = PR_FALSE;

        // Check if we have other selected cells
        nsIContent* previousCellNode =
          GetFirstSelectedContent(GetFirstCellRange());
        if (previousCellNode)
        {
          // We have at least 1 other selected cell

          // Check if new cell is already selected
          NS_ENSURE_STATE(mShell);
          nsIFrame  *cellFrame = mShell->GetPrimaryFrameFor(childContent);
          if (!cellFrame) return NS_ERROR_NULL_POINTER;
          result = cellFrame->GetSelected(&isSelected);
          if (NS_FAILED(result)) return result;
        }
        else
        {
          // No cells selected -- remove non-cell selection
          mDomSelections[index]->RemoveAllRanges();
        }
        mDragSelectingCells = PR_TRUE;    // Signal to start drag-cell-selection
        mSelectingTableCellMode = aTarget;
        // Set start for new drag-selection block (not appended)
        mStartSelectedCell = childContent;
        // The initial block end is same as the start
        mEndSelectedCell = childContent;
        
        if (isSelected)
        {
          // Remember this cell to (possibly) unselect it on mouseup
          mUnselectCellOnMouseUp = childContent;
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Saving mUnselectCellOnMouseUp\n");
#endif
        }
        else
        {
          // Select an unselected cell
          // but first remove existing selection if not in same table
          if (previousCellNode &&
              !IsInSameTable(previousCellNode, childContent))
          {
            mDomSelections[index]->RemoveAllRanges();
            // Reset selection mode that is cleared in RemoveAllRanges
            mSelectingTableCellMode = aTarget;
          }

          return SelectCellElement(childContent);
        }

        return NS_OK;
      }
      else if (aTarget == nsISelectionPrivate::TABLESELECTION_TABLE)
      {
        //TODO: We currently select entire table when clicked between cells,
        //  should we restrict to only around border?
        //  *** How do we get location data for cell and click?
        mDragSelectingCells = PR_FALSE;
        mStartSelectedCell = nsnull;
        mEndSelectedCell = nsnull;

        // Remove existing selection and select the table
        mDomSelections[index]->RemoveAllRanges();
        return CreateAndAddRange(aParentContent, aContentOffset);
      }
      else if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW || aTarget == nsISelectionPrivate::TABLESELECTION_COLUMN)
      {
#ifdef DEBUG_TABLE_SELECTION
printf("aTarget == %d\n", aTarget);
#endif

        // Start drag-selecting mode so multiple rows/cols can be selected
        // Note: Currently, nsFrame::GetDataForTableSelection
        //       will never call us for row or column selection on mouse down
        mDragSelectingCells = PR_TRUE;
      
        // Force new selection block
        mStartSelectedCell = nsnull;
        mDomSelections[index]->RemoveAllRanges();
        // Always do this AFTER RemoveAllRanges
        mSelectingTableCellMode = aTarget;
        return SelectRowOrColumn(childContent, aTarget);
      }
    }
    else
    {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Mouse UP event. mDragSelectingCells=%d, mStartSelectedCell=%d\n", mDragSelectingCells, mStartSelectedCell);
#endif
      // First check if we are extending a block selection
      PRInt32 rangeCount;
      result = mDomSelections[index]->GetRangeCount(&rangeCount);
      if (NS_FAILED(result)) 
        return result;

      if (rangeCount > 0 && aMouseEvent->isShift && 
          mAppendStartSelectedCell && mAppendStartSelectedCell != childContent)
      {
        // Shift key is down: append a block selection
        mDragSelectingCells = PR_FALSE;
        return SelectBlockOfCells(mAppendStartSelectedCell, childContent);
      }

      if (mDragSelectingCells)
        mAppendStartSelectedCell = mStartSelectedCell;
        
      mDragSelectingCells = PR_FALSE;
      mStartSelectedCell = nsnull;
      mEndSelectedCell = nsnull;

      // Any other mouseup actions require that Ctrl or Cmd key is pressed
      //  else stop table selection mode
      PRBool doMouseUpAction = PR_FALSE;
#ifdef XP_MACOSX
      doMouseUpAction = aMouseEvent->isMeta;
#else
      doMouseUpAction = aMouseEvent->isControl;
#endif
      if (!doMouseUpAction)
      {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Ending cell selection on mouseup: mAppendStartSelectedCell=%d\n", mAppendStartSelectedCell);
#endif
        return NS_OK;
      }
      // Unselect a cell only if it wasn't
      //  just selected on mousedown
      if( childContent == mUnselectCellOnMouseUp)
      {
        // Scan ranges to find the cell to unselect (the selection range to remove)
        // XXXbz it's really weird that this lives outside the loop, so once we
        // find one we keep looking at it even if we find no more cells...
        nsINode* previousCellParent = nsnull;
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Unselecting mUnselectCellOnMouseUp; rangeCount=%d\n", rangeCount);
#endif
        for( PRInt32 i = 0; i < rangeCount; i++)
        {
          // Strong reference, because sometimes we want to remove
          // this range, and then we might be the only owner.
          nsCOMPtr<nsIRange> range = mDomSelections[index]->GetRangeAt(i);
          if (!range) return NS_ERROR_NULL_POINTER;

          nsINode* parent = range->GetStartParent();
          if (!parent) return NS_ERROR_NULL_POINTER;

          PRInt32 offset = range->StartOffset();
          // Be sure previous selection is a table cell
          nsIContent* child = parent->GetChildAt(offset);
          if (child && IsCell(child))
            previousCellParent = parent;

          // We're done if we didn't find parent of a previously-selected cell
          if (!previousCellParent) break;
        
          if (previousCellParent == aParentContent && offset == aContentOffset)
          {
            // Cell is already selected
            if (rangeCount == 1)
            {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Unselecting single selected cell\n");
#endif
              // This was the only cell selected.
              // Collapse to "normal" selection inside the cell
              mStartSelectedCell = nsnull;
              mEndSelectedCell = nsnull;
              mAppendStartSelectedCell = nsnull;
              //TODO: We need a "Collapse to just before deepest child" routine
              // Even better, should we collapse to just after the LAST deepest child
              //  (i.e., at the end of the cell's contents)?
              return mDomSelections[index]->Collapse(childContent, 0);
            }
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Removing cell from multi-cell selection\n");
#endif
            // Unselecting the start of previous block 
            // XXX What do we use now!
            if (childContent == mAppendStartSelectedCell)
               mAppendStartSelectedCell = nsnull;

            // Deselect cell by removing its range from selection
            return mDomSelections[index]->RemoveRange(range);
          }
        }
        mUnselectCellOnMouseUp = nsnull;
      }
    }
  }
  return result;
}

nsresult
nsFrameSelection::SelectBlockOfCells(nsIContent *aStartCell, nsIContent *aEndCell)
{
  NS_ENSURE_TRUE(aStartCell, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aEndCell, NS_ERROR_NULL_POINTER);
  mEndSelectedCell = aEndCell;

  nsCOMPtr<nsIContent> startCell;
  nsresult result = NS_OK;

  // If new end cell is in a different table, do nothing
  nsIContent* table = IsInSameTable(aStartCell, aEndCell);
  if (!table) {
    return NS_OK;
  }

  // Get starting and ending cells' location in the cellmap
  PRInt32 startRowIndex, startColIndex, endRowIndex, endColIndex;
  result = GetCellIndexes(aStartCell, startRowIndex, startColIndex);
  if(NS_FAILED(result)) return result;
  result = GetCellIndexes(aEndCell, endRowIndex, endColIndex);
  if(NS_FAILED(result)) return result;

  // Check that |table| is a table.
  if (!GetTableLayout(table)) return NS_ERROR_FAILURE;

  PRInt32 curRowIndex, curColIndex;

  if (mDragSelectingCells)
  {
    // Drag selecting: remove selected cells outside of new block limits

    PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
    if (!mDomSelections[index])
      return NS_ERROR_NULL_POINTER;

    // Strong reference because we sometimes remove the range
    nsCOMPtr<nsIRange> range = GetFirstCellRange();
    nsIContent* cellNode = GetFirstSelectedContent(range);
    NS_PRECONDITION(!range || cellNode, "Must have cellNode if had a range");

    PRInt32 minRowIndex = PR_MIN(startRowIndex, endRowIndex);
    PRInt32 maxRowIndex = PR_MAX(startRowIndex, endRowIndex);
    PRInt32 minColIndex = PR_MIN(startColIndex, endColIndex);
    PRInt32 maxColIndex = PR_MAX(startColIndex, endColIndex);

    while (cellNode)
    {
      result = GetCellIndexes(cellNode, curRowIndex, curColIndex);
      if (NS_FAILED(result)) return result;

#ifdef DEBUG_TABLE_SELECTION
if (!range)
printf("SelectBlockOfCells -- range is null\n");
#endif
      if (range &&
          (curRowIndex < minRowIndex || curRowIndex > maxRowIndex || 
           curColIndex < minColIndex || curColIndex > maxColIndex))
      {
        mDomSelections[index]->RemoveRange(range);
        // Since we've removed the range, decrement pointer to next range
        mSelectedCellIndex--;
      }    
      range = GetNextCellRange();
      cellNode = GetFirstSelectedContent(range);
      NS_PRECONDITION(!range || cellNode, "Must have cellNode if had a range");
    }
  }

  nsCOMPtr<nsIDOMElement> cellElement;
  PRInt32 rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool  isSelected;

  // Note that we select block in the direction of user's mouse dragging,
  //  which means start cell may be after the end cell in either row or column
  PRInt32 row = startRowIndex;
  while(PR_TRUE)
  {
    PRInt32 col = startColIndex;
    while(PR_TRUE)
    {
      // Get TableLayout interface to access cell data based on cellmap location
      // frames are not ref counted, so don't use an nsCOMPtr
      nsITableLayout *tableLayoutObject = GetTableLayout(table);
      if (!tableLayoutObject) return NS_ERROR_FAILURE;

      result = tableLayoutObject->GetCellDataAt(row, col, *getter_AddRefs(cellElement),
                                                curRowIndex, curColIndex, rowSpan, colSpan, 
                                                actualRowSpan, actualColSpan, isSelected);
      if (NS_FAILED(result)) return result;

      NS_ASSERTION(actualColSpan, "!actualColSpan is 0!");

      // Skip cells that are spanned from previous locations or are already selected
      if (!isSelected && cellElement && row == curRowIndex && col == curColIndex)
      {
        nsCOMPtr<nsIContent> cellContent = do_QueryInterface(cellElement);
        result = SelectCellElement(cellContent);
        if (NS_FAILED(result)) return result;
      }
      // Done when we reach end column
      if (col == endColIndex) break;

      if (startColIndex < endColIndex)
        col ++;
      else
        col--;
    };
    if (row == endRowIndex) break;

    if (startRowIndex < endRowIndex)
      row++;
    else
      row--;
  };
  return result;
}

nsresult
nsFrameSelection::SelectRowOrColumn(nsIContent *aCellContent, PRUint32 aTarget)
{
  if (!aCellContent) return NS_ERROR_NULL_POINTER;

  nsIContent* table = GetParentTable(aCellContent);
  if (!table) return NS_ERROR_NULL_POINTER;

  // Get table and cell layout interfaces to access 
  //   cell data based on cellmap location
  // Frames are not ref counted, so don't use an nsCOMPtr
  nsITableLayout *tableLayout = GetTableLayout(table);
  if (!tableLayout) return NS_ERROR_FAILURE;
  nsITableCellLayout *cellLayout = GetCellLayout(aCellContent);
  if (!cellLayout) return NS_ERROR_FAILURE;

  // Get location of target cell:      
  PRInt32 rowIndex, colIndex, curRowIndex, curColIndex;
  nsresult result = cellLayout->GetCellIndexes(rowIndex, colIndex);
  if (NS_FAILED(result)) return result;

  // Be sure we start at proper beginning
  // (This allows us to select row or col given ANY cell!)
  if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW)
    colIndex = 0;
  if (aTarget == nsISelectionPrivate::TABLESELECTION_COLUMN)
    rowIndex = 0;

  nsCOMPtr<nsIDOMElement> cellElement;
  nsCOMPtr<nsIContent> firstCell;
  nsCOMPtr<nsIDOMElement> lastCell;
  PRInt32 rowSpan, colSpan, actualRowSpan, actualColSpan;
  PRBool isSelected;

  do {
    // Loop through all cells in column or row to find first and last
    result = tableLayout->GetCellDataAt(rowIndex, colIndex, *getter_AddRefs(cellElement),
                                        curRowIndex, curColIndex, rowSpan, colSpan, 
                                        actualRowSpan, actualColSpan, isSelected);
    if (NS_FAILED(result)) return result;
    if (cellElement)
    {
      NS_ASSERTION(actualRowSpan > 0 && actualColSpan> 0, "SelectRowOrColumn: Bad rowspan or colspan\n");
      if (!firstCell)
        firstCell = do_QueryInterface(cellElement);

      lastCell = cellElement;

      // Move to next cell in cellmap, skipping spanned locations
      if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW)
        colIndex += actualColSpan;
      else
        rowIndex += actualRowSpan;
    }
  }
  while (cellElement);

  // Use SelectBlockOfCells:
  // This will replace existing selection,
  //  but allow unselecting by dragging out of selected region
  if (firstCell && lastCell)
  {
    if (!mStartSelectedCell)
    {
      // We are starting a new block, so select the first cell
      result = SelectCellElement(firstCell);
      if (NS_FAILED(result)) return result;
      mStartSelectedCell = firstCell;
    }
    nsCOMPtr<nsIContent> lastCellContent = do_QueryInterface(lastCell);
    result = SelectBlockOfCells(mStartSelectedCell, lastCellContent);

    // This gets set to the cell at end of row/col, 
    //   but we need it to be the cell under cursor
    mEndSelectedCell = aCellContent;
    return result;
  }

#if 0
// This is a more efficient strategy that appends row to current selection,
//  but doesn't allow dragging OFF of an existing selection to unselect!
  do {
    // Loop through all cells in column or row
    result = tableLayout->GetCellDataAt(rowIndex, colIndex,
                                        getter_AddRefs(cellElement),
                                        curRowIndex, curColIndex,
                                        rowSpan, colSpan,
                                        actualRowSpan, actualColSpan,
                                        isSelected);
    if (NS_FAILED(result)) return result;
    // We're done when cell is not found
    if (!cellElement) break;


    // Check spans else we infinitely loop
    NS_ASSERTION(actualColSpan, "actualColSpan is 0!");
    NS_ASSERTION(actualRowSpan, "actualRowSpan is 0!");
    
    // Skip cells that are already selected or span from outside our region
    if (!isSelected && rowIndex == curRowIndex && colIndex == curColIndex)
    {
      result = SelectCellElement(cellElement);
      if (NS_FAILED(result)) return result;
    }
    // Move to next row or column in cellmap, skipping spanned locations
    if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW)
      colIndex += actualColSpan;
    else
      rowIndex += actualRowSpan;
  }
  while (cellElement);
#endif

  return NS_OK;
}

nsIContent*
nsFrameSelection::GetFirstCellNodeInRange(nsIRange *aRange) const
{
  if (!aRange) return nsnull;

  nsINode* startParent = aRange->GetStartParent();
  if (!startParent)
    return nsnull;

  PRInt32 offset = aRange->StartOffset();

  nsIContent* childContent = startParent->GetChildAt(offset);
  if (!childContent)
    return nsnull;
  // Don't return node if not a cell
  if (!IsCell(childContent))
    return nsnull;

  return childContent;
}

nsIRange*
nsFrameSelection::GetFirstCellRange()
{
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return nsnull;

  nsIRange* firstRange = mDomSelections[index]->GetRangeAt(0);
  if (!GetFirstCellNodeInRange(firstRange)) {
    return nsnull;
  }

  // Setup for next cell
  mSelectedCellIndex = 1;

  return firstRange;
}

nsIRange*
nsFrameSelection::GetNextCellRange()
{
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return nsnull;

  nsIRange* range = mDomSelections[index]->GetRangeAt(mSelectedCellIndex);

  // Get first node in next range of selection - test if it's a cell
  if (!GetFirstCellNodeInRange(range)) {
    return nsnull;
  }

  // Setup for next cell
  mSelectedCellIndex++;

  return range;
}

nsresult
nsFrameSelection::GetCellIndexes(nsIContent *aCell,
                                 PRInt32    &aRowIndex,
                                 PRInt32    &aColIndex)
{
  if (!aCell) return NS_ERROR_NULL_POINTER;

  aColIndex=0; // initialize out params
  aRowIndex=0;

  nsITableCellLayout *cellLayoutObject = GetCellLayout(aCell);
  if (!cellLayoutObject)  return NS_ERROR_FAILURE;
  return cellLayoutObject->GetCellIndexes(aRowIndex, aColIndex);
}

nsIContent*
nsFrameSelection::IsInSameTable(nsIContent  *aContent1,
                                nsIContent  *aContent2) const
{
  if (!aContent1 || !aContent2) return PR_FALSE;
  
  nsIContent* tableNode1 = GetParentTable(aContent1);
  nsIContent* tableNode2 = GetParentTable(aContent2);

  // Must be in the same table.  Note that we want to return false for
  // the test if both tables are null.
  return (tableNode1 == tableNode2) ? tableNode1 : nsnull;
}

nsIContent*
nsFrameSelection::GetParentTable(nsIContent *aCell) const
{
  if (!aCell) {
    return nsnull;
  }

  for (nsIContent* parent = aCell->GetParent(); parent;
       parent = parent->GetParent()) {
    if (parent->Tag() == nsGkAtoms::table &&
        parent->IsNodeOfType(nsINode::eHTML)) {
      return parent;
    }
  }

  return nsnull;
}

nsresult
nsFrameSelection::SelectCellElement(nsIContent *aCellElement)
{
  nsIContent *parent = aCellElement->GetParent();

  // Get child offset
  PRInt32 offset = parent->IndexOf(aCellElement);

  return CreateAndAddRange(parent, offset);
}

nsresult
nsTypedSelection::getTableCellLocationFromRange(nsIRange *aRange, PRInt32 *aSelectionType, PRInt32 *aRow, PRInt32 *aCol)
{
  if (!aRange || !aSelectionType || !aRow || !aCol)
    return NS_ERROR_NULL_POINTER;

  *aSelectionType = nsISelectionPrivate::TABLESELECTION_NONE;
  *aRow = 0;
  *aCol = 0;

  // Must have access to frame selection to get cell info
  if (!mFrameSelection) return NS_OK;

  nsresult result = GetTableSelectionType(aRange, aSelectionType);
  if (NS_FAILED(result)) return result;
  
  // Don't fail if range does not point to a single table cell,
  //  let aSelectionType tell user if we don't have a cell
  if (*aSelectionType  != nsISelectionPrivate::TABLESELECTION_CELL)
    return NS_OK;

  // Get the child content (the cell) pointed to by starting node of range
  // We do minimal checking since GetTableSelectionType assures
  //   us that this really is a table cell
  nsCOMPtr<nsIContent> content = do_QueryInterface(aRange->GetStartParent());
  if (!content)
    return NS_ERROR_FAILURE;

  nsIContent *child = content->GetChildAt(aRange->StartOffset());
  if (!child)
    return NS_ERROR_FAILURE;

  //Note: This is a non-ref-counted pointer to the frame
  nsITableCellLayout *cellLayout = mFrameSelection->GetCellLayout(child);
  if (NS_FAILED(result))
    return result;
  if (!cellLayout)
    return NS_ERROR_FAILURE;

  return cellLayout->GetCellIndexes(*aRow, *aCol);
}

nsresult
nsTypedSelection::addTableCellRange(nsIRange *aRange, PRBool *aDidAddRange,
                                    PRInt32 *aOutIndex)
{  
  if (!aDidAddRange || !aOutIndex)
    return NS_ERROR_NULL_POINTER;

  *aDidAddRange = PR_FALSE;
  *aOutIndex = -1;

  if (!mFrameSelection)
    return NS_OK;

  if (!aRange)
    return NS_ERROR_NULL_POINTER;

  nsresult result;

  // Get if we are adding a cell selection and the row, col of cell if we are
  PRInt32 newRow, newCol, tableMode;
  result = getTableCellLocationFromRange(aRange, &tableMode, &newRow, &newCol);
  if (NS_FAILED(result)) return result;
  
  // If not adding a cell range, we are done here
  if (tableMode != nsISelectionPrivate::TABLESELECTION_CELL)
  {
    mFrameSelection->mSelectingTableCellMode = tableMode;
    // Don't fail if range isn't a selected cell, aDidAddRange tells caller if we didn't proceed
    return NS_OK;
  }
  
  // Set frame selection mode only if not already set to a table mode
  //  so we don't loose the select row and column flags (not detected by getTableCellLocation)
  if (mFrameSelection->mSelectingTableCellMode == TABLESELECTION_NONE)
    mFrameSelection->mSelectingTableCellMode = tableMode;

  *aDidAddRange = PR_TRUE;
  return AddItem(aRange, aOutIndex);
}

//TODO: Figure out TABLESELECTION_COLUMN and TABLESELECTION_ALLCELLS
NS_IMETHODIMP
nsTypedSelection::GetTableSelectionType(nsIDOMRange* aRange,
                                        PRInt32* aTableSelectionType)
{
  nsCOMPtr<nsIRange> range = do_QueryInterface(aRange);
  return GetTableSelectionType(range, aTableSelectionType);
}

nsresult
nsTypedSelection::GetTableSelectionType(nsIRange* aRange,
                                        PRInt32* aTableSelectionType)
{
  if (!aRange || !aTableSelectionType)
    return NS_ERROR_NULL_POINTER;
  
  *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_NONE;
 
  // Must have access to frame selection to get cell info
  if(!mFrameSelection) return NS_OK;

  nsINode* startNode = aRange->GetStartParent();
  if (!startNode) return NS_ERROR_FAILURE;
  
  nsINode* endNode = aRange->GetEndParent();
  if (!endNode) return NS_ERROR_FAILURE;

  // Not a single selected node
  if (startNode != endNode) return NS_OK;

  PRInt32 startOffset = aRange->StartOffset();
  PRInt32 endOffset = aRange->EndOffset();

  // Not a single selected node
  if ((endOffset - startOffset) != 1)
    return NS_OK;

  if (!startNode->IsNodeOfType(nsINode::eHTML)) {
    // Implies a check for being an element; if we ever make this work
    // for non-HTML, need to keep checking for elements.
    return NS_OK;
  }

  nsIAtom *tag = static_cast<nsIContent*>(startNode)->Tag();

  if (tag == nsGkAtoms::tr)
  {
    *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_CELL;
  }
  else //check to see if we are selecting a table or row (column and all cells not done yet)
  {
    nsIContent *child = startNode->GetChildAt(startOffset);
    if (!child)
      return NS_ERROR_FAILURE;

    tag = child->Tag();

    if (tag == nsGkAtoms::table)
      *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_TABLE;
    else if (tag == nsGkAtoms::tr)
      *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_ROW;
  }

  return NS_OK;
}

nsresult
nsFrameSelection::CreateAndAddRange(nsINode *aParentNode, PRInt32 aOffset)
{
  if (!aParentNode) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIRange> range = new nsRange();
  if (!range) return NS_ERROR_OUT_OF_MEMORY;

  // Set range around child at given offset
  nsresult result = range->SetStart(aParentNode, aOffset);
  if (NS_FAILED(result)) return result;
  result = range->SetEnd(aParentNode, aOffset+1);
  if (NS_FAILED(result)) return result;
  
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[index]->AddRange(range);
}

// End of Table Selection

void
nsFrameSelection::SetAncestorLimiter(nsIContent *aLimiter)
{
  if (mAncestorLimiter != aLimiter) {
    mAncestorLimiter = aLimiter;
    PRInt8 index =
      GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
    if (!mDomSelections[index])
      return;

    if (!IsValidSelectionPoint(this, mDomSelections[index]->GetFocusNode())) {
      ClearNormalSelection();
      if (mAncestorLimiter) {
        PostReason(nsISelectionListener::NO_REASON);
        TakeFocus(mAncestorLimiter, 0, 0, HINTLEFT, PR_FALSE, PR_FALSE);
      }
    }
  }
}

//END nsFrameSelection methods


//BEGIN nsISelection interface implementations



nsresult
nsFrameSelection::DeleteFromDocument()
{
  nsresult res;

  // If we're already collapsed, then set ourselves to include the
  // last item BEFORE the current range, rather than the range itself,
  // before we do the delete.
  PRBool isCollapsed;
  PRInt8 index = GetIndexFromSelectionType(nsISelectionController::SELECTION_NORMAL);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  mDomSelections[index]->GetIsCollapsed( &isCollapsed);
  if (isCollapsed)
  {
    // If the offset is positive, then it's easy:
    if (mDomSelections[index]->GetFocusOffset() > 0)
    {
      mDomSelections[index]->Extend(mDomSelections[index]->GetFocusNode(), mDomSelections[index]->GetFocusOffset() - 1);
    }
    else
    {
      // Otherwise it's harder, have to find the previous node
      printf("Sorry, don't know how to delete across frame boundaries yet\n");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  // Get an iterator
  nsSelectionIterator iter(mDomSelections[index]);
  res = iter.First();
  if (NS_FAILED(res))
    return res;

  while (iter.IsDone())
  {
    nsCOMPtr<nsIRange> range = iter.CurrentItem();
    res = range->DeleteContents();
    if (NS_FAILED(res))
      return res;
    iter.Next();
  }

  // Collapse to the new location.
  // If we deleted one character, then we move back one element.
  // FIXME  We don't know how to do this past frame boundaries yet.
  if (isCollapsed)
    mDomSelections[index]->Collapse(mDomSelections[index]->GetAnchorNode(), mDomSelections[index]->GetAnchorOffset()-1);
  else if (mDomSelections[index]->GetAnchorOffset() > 0)
    mDomSelections[index]->Collapse(mDomSelections[index]->GetAnchorNode(), mDomSelections[index]->GetAnchorOffset());
#ifdef DEBUG
  else
    printf("Don't know how to set selection back past frame boundary\n");
#endif

  return NS_OK;
}

void
nsFrameSelection::SetDelayedCaretData(nsMouseEvent *aMouseEvent)
{
  if (aMouseEvent)
  {
    mDelayedMouseEventValid = PR_TRUE;
    mDelayedMouseEvent      = *aMouseEvent;

    // Don't cache the widget.  We don't need it and it could go away.
    mDelayedMouseEvent.widget = nsnull;
  }
  else
    mDelayedMouseEventValid = PR_FALSE;
}

nsMouseEvent*
nsFrameSelection::GetDelayedCaretData()
{
  if (mDelayedMouseEventValid)
    return &mDelayedMouseEvent;
  
  return nsnull;
}

//END nsISelection interface implementations

#if 0
#pragma mark -
#endif

// nsTypedSelection implementation

// note: this can return a nil anchor node

nsTypedSelection::nsTypedSelection()
  : mCachedOffsetForFrame(nsnull)
  , mDirection(eDirNext)
  , mType(nsISelectionController::SELECTION_NORMAL)
{
}

nsTypedSelection::nsTypedSelection(nsFrameSelection *aList)
  : mFrameSelection(aList)
  , mCachedOffsetForFrame(nsnull)
  , mDirection(eDirNext)
  , mType(nsISelectionController::SELECTION_NORMAL)
{
}

nsTypedSelection::~nsTypedSelection()
{
  setAnchorFocusRange(-1);

  if (mAutoScrollTimer) {
    mAutoScrollTimer->Stop();
    mAutoScrollTimer = nsnull;
  }

  mScrollEvent.Revoke();

  if (mCachedOffsetForFrame) {
    delete mCachedOffsetForFrame;
    mCachedOffsetForFrame = nsnull;
  }
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsTypedSelection)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsTypedSelection)
  tmp->RemoveAllRanges();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFrameSelection)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTypedSelection)
  {
    PRUint32 i, count = tmp->mRanges.Length();
    for (i = 0; i < count; ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRanges[i].mRange)
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAnchorFocusRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFrameSelection)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// QueryInterface implementation for nsTypedSelection
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTypedSelection)
  NS_INTERFACE_MAP_ENTRY(nsISelection)
  NS_INTERFACE_MAP_ENTRY(nsISelection2)
  NS_INTERFACE_MAP_ENTRY(nsISelectionPrivate)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISelection)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Selection)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTypedSelection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTypedSelection)

NS_IMETHODIMP
nsTypedSelection::SetPresShell(nsIPresShell *aPresShell)
{
  mPresShellWeak = do_GetWeakReference(aPresShell);
  return NS_OK;
}



NS_IMETHODIMP
nsTypedSelection::GetAnchorNode(nsIDOMNode** aAnchorNode)
{
  nsINode* anchorNode = GetAnchorNode();
  if (anchorNode) {
    return CallQueryInterface(anchorNode, aAnchorNode);
  }

  *aAnchorNode = nsnull;
  return NS_OK;
}

nsINode*
nsTypedSelection::GetAnchorNode()
{
  if (!mAnchorFocusRange)
    return nsnull;
   
  if (GetDirection() == eDirNext) {
    return mAnchorFocusRange->GetStartParent();
  }

  return mAnchorFocusRange->GetEndParent();
}

NS_IMETHODIMP
nsTypedSelection::GetAnchorOffset(PRInt32* aAnchorOffset)
{
  *aAnchorOffset = GetAnchorOffset();
  return NS_OK;
}

// note: this can return a nil focus node
NS_IMETHODIMP
nsTypedSelection::GetFocusNode(nsIDOMNode** aFocusNode)
{
  nsINode* focusNode = GetFocusNode();
  if (focusNode) {
    return CallQueryInterface(focusNode, aFocusNode);
  }

  *aFocusNode = nsnull;
  return NS_OK;
}

nsINode*
nsTypedSelection::GetFocusNode()
{
  if (!mAnchorFocusRange)
    return nsnull;

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->GetEndParent();
  }

  return mAnchorFocusRange->GetStartParent();
}

NS_IMETHODIMP nsTypedSelection::GetFocusOffset(PRInt32* aFocusOffset)
{
  *aFocusOffset = GetFocusOffset();
  return NS_OK;
}

void nsTypedSelection::setAnchorFocusRange(PRInt32 indx)
{
  if (indx >= (PRInt32)mRanges.Length())
    return;
  if (indx < 0) //release all
  {
    mAnchorFocusRange = nsnull;
  }
  else{
    mAnchorFocusRange = mRanges[indx].mRange;
  }
}

PRInt32
nsTypedSelection::GetAnchorOffset()
{
  if (!mAnchorFocusRange)
    return 0;

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->StartOffset();
  }

  return mAnchorFocusRange->EndOffset();
}

PRInt32
nsTypedSelection::GetFocusOffset()
{
  if (!mAnchorFocusRange)
    return 0;

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->EndOffset();
  }

  return mAnchorFocusRange->StartOffset();
}

static PRInt32
CompareToRangeStart(nsINode* aCompareNode, PRInt32 aCompareOffset,
                    nsIRange* aRange)
{
  return nsContentUtils::ComparePoints(aCompareNode, aCompareOffset,
                                       aRange->GetStartParent(),
                                       aRange->StartOffset());
}

static PRInt32
CompareToRangeEnd(nsINode* aCompareNode, PRInt32 aCompareOffset,
                  nsIRange* aRange)
{
  return nsContentUtils::ComparePoints(aCompareNode, aCompareOffset,
                                       aRange->GetEndParent(),
                                       aRange->EndOffset());
}

#ifdef DEBUG
// checks the indices to make sure the range bookkeeping is up-to-date, useful
// for debugging to make sure everything is consistent
PRBool
nsTypedSelection::ValidateRanges()
{
  if (mRanges.Length() != mRangeEndings.Length()) {
    NS_NOTREACHED("Lengths don't match");
    return PR_FALSE;
  }

  // check the main array and make sure they refer to the correct sorted ones
  PRUint32 i;
  for (i = 0; i < mRanges.Length(); i ++) {
    if (mRanges[i].mEndIndex < 0 ||
        mRanges[i].mEndIndex >= (PRInt32)mRangeEndings.Length()) {
      NS_NOTREACHED("Sorted index is out of bounds");
      return PR_FALSE;
    }
    if (mRangeEndings[mRanges[i].mEndIndex] != (PRInt32)i) {
      NS_NOTREACHED("Beginning or ending isn't correct");
      return PR_FALSE;
    }
  }

  // check the other array to make sure they all refer to valid indices
  for (i = 0; i < mRangeEndings.Length(); i ++) {
    if (mRangeEndings[i] < 0 ||
        mRangeEndings[i] >= (PRInt32)mRanges.Length()) {
      NS_NOTREACHED("Ending refers to invalid index");
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}
#endif

// nsTypedSelection::FindInsertionPoint
//
//    Binary searches the sorted array of ranges for the insertion point for
//    the given node/offset. The given comparator is used, and the index where
//    the point should appear in the array is placed in *aInsertionPoint;
//
//    If the remapping array is given, we'll find the position in that array
//    for the insertion, where that array contains indices that reference
//    the range array. This can be NULL to not use a remapping array.
//
//    If there is item(s) in the array equal to the input point, we will return
//    a random index between or adjacent to this item(s).

PRInt32
nsTypedSelection::FindInsertionPoint(
    const nsTArray<PRInt32>* aRemappingArray,
    nsINode* aPointNode, PRInt32 aPointOffset,
    PRInt32 (*aComparator)(nsINode*,PRInt32,nsIRange*))
{
  NS_ASSERTION(!aRemappingArray || aRemappingArray->Length() == mRanges.Length(),
               "Remapping array must have the same entries as the range array");

  PRInt32 beginSearch = 0;
  PRInt32 endSearch = mRanges.Length(); // one beyond what to check
  while (endSearch - beginSearch > 0) {
    PRInt32 center = (endSearch - beginSearch) / 2 + beginSearch;

    nsIRange* range;
    if (aRemappingArray)
      range = mRanges[(*aRemappingArray)[center]].mRange;
    else
      range = mRanges[center].mRange;

    PRInt32 cmp = aComparator(aPointNode, aPointOffset, range);

    if (cmp < 0) {        // point < cur
      endSearch = center;
    } else if (cmp > 0) { // point > cur
      beginSearch = center + 1;
    } else {              // found match, done
      beginSearch = center;
      break;
    }
  }
  return beginSearch;
}

nsresult
nsTypedSelection::AddItem(nsIRange *aItem, PRInt32 *aOutIndex)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (!aItem->IsPositioned())
    return NS_ERROR_UNEXPECTED;
  if (aOutIndex)
    *aOutIndex = -1;
  
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");

  // a common case is that we have no ranges yet
  if (mRanges.Length() == 0) {
    if (! mRanges.AppendElement(RangeData(aItem, 0)))
      return NS_ERROR_OUT_OF_MEMORY;
    if (! mRangeEndings.AppendElement(0)) {
      mRanges.Clear();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (aOutIndex)
      *aOutIndex = 0;
    return NS_OK;
  }

  PRInt32 beginInsertionPoint =
    FindInsertionPoint(nsnull, aItem->GetStartParent(), aItem->StartOffset(),
                       CompareToRangeStart);

  if (aOutIndex)
    *aOutIndex = beginInsertionPoint;
  
  // XXX PERFORMANCE: 99% of the time, the beginning array and the ending array
  // will be the same because the ranges do not overlap. We could save a few
  // compares (which can be expensive) in this common case by special casing
  // this.

  // make sure that this range is not already in the selection
  PRInt32 index = FindRangeGivenPoint(aItem->GetStartParent(),
                                      aItem->StartOffset(),
                                      aItem->GetEndParent(),
                                      aItem->EndOffset(),
                                      beginInsertionPoint);
  if (index >= 0) {
    // silently succeed, this range is already in the selection
    if (aOutIndex)
      *aOutIndex = index;
    return NS_OK;
  }

  PRInt32 endInsertionPoint =
    FindInsertionPoint(&mRangeEndings, aItem->GetEndParent(),
                       aItem->EndOffset(), CompareToRangeEnd);

  // insert the range, being careful to revert everything on error to keep
  // consistency
  if (! mRanges.InsertElementAt(beginInsertionPoint,
        RangeData(aItem, endInsertionPoint))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (! mRangeEndings.InsertElementAt(endInsertionPoint, beginInsertionPoint)) {
    mRanges.RemoveElementAt(beginInsertionPoint);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  // adjust the end indices that point to the main list
  PRUint32 i;
  for (i = 0; i < mRangeEndings.Length(); i ++) {
    if (mRangeEndings[i] >= beginInsertionPoint)
      mRangeEndings[i] ++;
  }

  // the last loop updated the inserted index as well, so we need to put it
  // back (this saves a comparison in that loop)
  mRangeEndings[endInsertionPoint] = beginInsertionPoint;

  // adjust the begin/end indices
  for (i = endInsertionPoint + 1; i < mRangeEndings.Length(); i ++)
    mRanges[mRangeEndings[i]].mEndIndex = i;

  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");
  return NS_OK;
}


nsresult
nsTypedSelection::RemoveItem(nsIRange *aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");

  // Find the range's index & remove it. We could use FindInsertionPoint to
  // get O(log n) time, but that requires many expensive DOM comparisons.
  // For even several thousand items, this is probably faster because the
  // comparisons are so fast.
  PRInt32 idx = -1;
  PRUint32 i;
  for (i = 0; i < mRanges.Length(); i ++) {
    if (mRanges[i].mRange == aItem) {
      idx = (PRInt32)i;
      break;
    }
  }
  if (idx < 0)
    return NS_ERROR_INVALID_ARG;
  mRanges.RemoveElementAt(idx);

  // need to update the range ending list to reflect the removed item
  PRInt32 endingIndex = -1;
  for (i = 0; i < mRangeEndings.Length(); i ++) {
    if (mRangeEndings[i] == idx)
      endingIndex = i;
    if (mRangeEndings[i] > idx)
      mRangeEndings[i] --;
  }
  NS_ASSERTION(endingIndex >= 0 && endingIndex < (PRInt32)mRangeEndings.Length(),
               "Index not found in ending list");

  // remove from the sorted lists
  mRangeEndings.RemoveElementAt(endingIndex);

  // adjust indices of the RangeData structures
  for (i = endingIndex; i < mRangeEndings.Length(); i ++)
    mRanges[mRangeEndings[i]].mEndIndex = i;

  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");
  return NS_OK;
}

nsresult
nsTypedSelection::RemoveCollapsedRanges()
{
  PRUint32 i = 0;
  while (i < mRanges.Length()) {
    if (mRanges[i].mRange->Collapsed()) {
      nsresult rv = RemoveItem(mRanges[i].mRange);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      ++i;
    }
  }
  return NS_OK;
}

nsresult
nsTypedSelection::Clear(nsPresContext* aPresContext)
{
  setAnchorFocusRange(-1);

  for (PRInt32 i = 0; i < (PRInt32)mRanges.Length(); i ++) {
    selectFrames(aPresContext, mRanges[i].mRange, 0);
  }
  mRanges.Clear();
  mRangeEndings.Clear();

  // Reset direction so for more dependable table selection range handling
  SetDirection(eDirNext);

  // If this was an ATTENTION selection, change it back to normal now
  if (mFrameSelection &&
      mFrameSelection->GetDisplaySelection() ==
      nsISelectionController::SELECTION_ATTENTION) {
    mFrameSelection->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  }

  return NS_OK;
}

// nsTypedSelection::MoveIndexToFirstMatch
//
//    This adjusts the given index backwards in the range array so that it
//    points to the first match of (node,offset). There might be any number
//    of identical sequencial items in the array and the index can initially
//    point to any one of them. When complete, the index will be moved to
//    point to the first one of these. If the index does not point to a
//    match of (node,offset) or there is only one match, the index will be
//    untouched.
//
//    If there are multiple ranges beginning at the requested position, we'll
//    get a random index in between those from FindInsertionPoint. We want the
//    index to be at the first match in the array.
//
//    If the remapping array (sorted list containing indices into mRanges)
//    is given, we'll use that for sorting and consider the index into
//    that array. If NULL, we'll just use mRanges directly.
//
//    If aUseBeginning is set we'll compare to the range beginnings in the
//    selection. If false, we'll compare to range endings.

void
nsTypedSelection::MoveIndexToFirstMatch(PRInt32* aIndex, nsINode* aNode,
                                        PRInt32 aOffset,
                                        const nsTArray<PRInt32>* aRemappingArray,
                                        PRBool aUseBeginning)
{
  while (*aIndex > 0) {
    nsIRange* range;
    if (aRemappingArray)
      range = mRanges[(*aRemappingArray)[(*aIndex) - 1]].mRange;
    else
      range = mRanges[(*aIndex) - 1].mRange;

    nsINode* curNode;
    PRInt32 curOffset;
    if (aUseBeginning) {
      curNode = range->GetStartParent();
      curOffset = range->StartOffset();
    } else {
      curNode = range->GetEndParent();
      curOffset = range->EndOffset();
    }

    if (curNode != aNode)
      break; // not a match
    if (curOffset != aOffset)
      break; // not a match

    // the previous node matches, go back one
    (*aIndex) --;
  }
}

// nsTypedSelection::MoveIndexToNextMismatch
//
//    The same as MoveIndexToFirstMatch but increments the index until it
//    points to something that doesn't match (node,offset).
//
//    If there is one or more range ending at the requested position, we
//    want to start checking the first one that ends inside the range. This
//    moves the given index to point to the first index inside the range.
//    It may be one past the last item in the array.
//
//    If the requested DOM position is '9', and the caller got index 3
//    from FindInsertionPoint with points in the range array:
//       6  8  9  9  9  13  19
//                ^
//                input
//    The input point will point to a random '9' in the array, or possibly
//    the '13', which are all valid points for a new '9' to live in this array.
//    This function will move the index to point to the '13' in this case,
//    which is the first offset not matching the input position.
//
//    See MoveIndexToFirstMatch for the meaning of aRemappingArray and
//    aUseBeginning.

void
nsTypedSelection::MoveIndexToNextMismatch(PRInt32* aIndex, nsINode* aNode,
                                          PRInt32 aOffset,
                                          const nsTArray<PRInt32>* aRemappingArray,
                                          PRBool aUseBeginning)
{
  while (*aIndex < (PRInt32)mRanges.Length()) {
    nsIRange* range;
    if (aRemappingArray)
      range = mRanges[(*aRemappingArray)[*aIndex]].mRange;
    else
      range = mRanges[*aIndex].mRange;

    nsINode* curNode;
    PRInt32 curOffset;
    if (aUseBeginning) {
      curNode = range->GetStartParent();
      curOffset = range->StartOffset();
    } else {
      curNode = range->GetEndParent();
      curOffset = range->EndOffset();
    }

    if (curNode != aNode)
      break; // mismatch
    if (curOffset != aOffset)
      break; // mismatch

    // this node matches, go to the next one
    (*aIndex) ++;
  }
}

NS_IMETHODIMP
nsTypedSelection::GetType(PRInt16 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;

  return NS_OK;
}

// nsTypedSelection::GetRangesForInterval
//
//    XPCOM wrapper for the COMArray version

NS_IMETHODIMP
nsTypedSelection::GetRangesForInterval(nsIDOMNode* aBeginNode, PRInt32 aBeginOffset,
                                       nsIDOMNode* aEndNode, PRInt32 aEndOffset,
                                       PRBool aAllowAdjacent,
                                       PRUint32 *aResultCount,
                                       nsIDOMRange ***aResults)
{
  if (! aBeginNode || ! aEndNode || ! aResultCount || ! aResults)
    return NS_ERROR_NULL_POINTER;

  *aResultCount = 0;
  *aResults = nsnull;

  nsCOMArray<nsIDOMRange> results;
  nsresult rv = GetRangesForIntervalCOMArray(aBeginNode, aBeginOffset,
                                             aEndNode, aEndOffset,
                                             aAllowAdjacent,
                                             &results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (results.Count() == 0)
    return NS_OK;

  *aResults = static_cast<nsIDOMRange**>
                         (nsMemory::Alloc(sizeof(nsIDOMRange*) * results.Count()));
  NS_ENSURE_TRUE(*aResults, NS_ERROR_OUT_OF_MEMORY);

  *aResultCount = results.Count();
  for (PRInt32 i = 0; i < results.Count(); i ++)
    NS_ADDREF((*aResults)[i] = results[i]);
  return NS_OK;
}

// nsTypedSelection::GetRangesForIntervalCOMArray
//
//    Fills a COM array with the ranges overlapping the range specified by
//    the given endpoints. Ranges in the selection exactly adjacent to the
//    input range are not returned unless aAllowAdjacent is set.

NS_IMETHODIMP
nsTypedSelection::GetRangesForIntervalCOMArray(nsIDOMNode* aBeginNode, PRInt32 aBeginOffset,
                                               nsIDOMNode* aEndNode, PRInt32 aEndOffset,
                                               PRBool aAllowAdjacent,
                                               nsCOMArray<nsIDOMRange>* aRanges)
{
  nsCOMPtr<nsINode> begin = do_QueryInterface(aBeginNode);
  nsCOMPtr<nsINode> end = do_QueryInterface(aEndNode);
  nsCOMArray<nsIRange> ranges;
  nsresult rv = GetRangesForIntervalCOMArray(begin, aBeginOffset,
                                             end, aEndOffset,
                                             aAllowAdjacent, &ranges);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRInt32 i = 0; i < ranges.Count(); ++i) {
    nsCOMPtr<nsIDOMRange> r = do_QueryInterface(ranges[i]);
    if (!aRanges->AppendObject(r)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

nsresult
nsTypedSelection::GetRangesForIntervalCOMArray(nsINode* aBeginNode, PRInt32 aBeginOffset,
                                               nsINode* aEndNode, PRInt32 aEndOffset,
                                               PRBool aAllowAdjacent,
                                               nsCOMArray<nsIRange>* aRanges)
{
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");
  aRanges->Clear();
  if (mRanges.Length() == 0)
    return NS_OK;

  // Ranges that begin after the checked range, and ranges that end before
  // the checked range can be discarded. The beginning index is the offset
  // into the beginning array of the first item we DON'T have to check.

  // ...index into the beginning array that is the FIRST ITEM OUTSIDE
  //    OUR RANGE
  PRInt32 beginningIndex =
    FindInsertionPoint(nsnull, aEndNode, aEndOffset, &CompareToRangeStart);
  if (beginningIndex == 0)
    return NS_OK; // optimization: all ranges are after us

  // ...index into the ending array that is the FIRST ITEM WE WANT TO CHECK
  PRInt32 endingIndex =
    FindInsertionPoint(&mRangeEndings, aBeginNode, aBeginOffset,
                       &CompareToRangeEnd);
  if (endingIndex == (PRInt32)mRangeEndings.Length())
    return NS_OK; // optimization: all ranges are before us

  // adjust the indices in case of exact matches, FindInsertionPoint will
  // give us a random index that would still be sorted if there is a match
  if (aAllowAdjacent) {
    // include adjacent points
    //
    // Recall that we want all things in mRangeEndings (indexed by endingIndex)
    // before the requested beginning, and everything in mRanges (indexed by
    // beginningIndex) after the requested ending.
    //
    // 1 3 5 5 5 8 9 10 10 10 11 12  <-- imaginary DOM positions of ranges
    //       ^          ^            <-- we have this
    //     ^                  ^      <-- compute this range
    //  endingIndex   beginningIndex
    MoveIndexToFirstMatch(&endingIndex, aBeginNode, aBeginOffset,
                          &mRangeEndings, PR_FALSE);
    MoveIndexToNextMismatch(&beginningIndex, aEndNode, aEndOffset,
                            nsnull, PR_TRUE);
  } else {
    // exclude adjacent points, see previous case
    // 1 3 5 5 5 8 9 10 10 10 11 12  <-- imaginary DOM positions of ranges
    //       ^          ^            <-- we have this
    //           ^   ^               <-- compute this range
    // endingIndex   beginningIndex
    MoveIndexToNextMismatch(&endingIndex, aBeginNode, aBeginOffset,
                            &mRangeEndings, PR_FALSE);
    MoveIndexToFirstMatch(&beginningIndex, aEndNode, aEndOffset,
                          nsnull, PR_TRUE);
  }

  // check for overlaps in the two ranges by linearly searching the smallest
  // set of matches
  if (beginningIndex > (PRInt32)mRangeEndings.Length() - endingIndex) {
    // check ending array because its smaller
    for (PRInt32 i = endingIndex; i < (PRInt32)mRangeEndings.Length(); i ++) {
      if (mRangeEndings[i] < beginningIndex) {
        if (! aRanges->AppendObject(mRanges[mRangeEndings[i]].mRange))
          return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  } else {
    for (PRInt32 i = 0; i < beginningIndex; i ++) {
      if (mRanges[i].mEndIndex >= endingIndex) {
        if (! aRanges->AppendObject(mRanges[i].mRange))
          return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return NS_OK;
}

// RangeMatches*Point
//
//    Compares the range beginning or ending point, and returns true if it
//    exactly matches the given DOM point.

static inline PRBool
RangeMatchesBeginPoint(nsIRange* aRange, nsINode* aNode, PRInt32 aOffset)
{
  return aRange->GetStartParent() == aNode && aRange->StartOffset() == aOffset;
}

static inline PRBool
RangeMatchesEndPoint(nsIRange* aRange, nsINode* aNode, PRInt32 aOffset)
{
  return aRange->GetEndParent() == aNode && aRange->EndOffset() == aOffset;
}


// nsTypedSelection::FindRangeGivenPoint
//
//    Searches for the range matching the exact given range points. We search
//    in the array of beginnings, and start from the given index. This index
//    should be the result of FindInsertionPoint, which will return any index
//    within a range of identical ones.
//
//    Therefore, this function searches backwards and forwards from that point
//    of all matching beginning points, and then compares the ending points to
//    find a match. Returns the index of the match if a match was found, -1 if 
//    not.

PRInt32
nsTypedSelection::FindRangeGivenPoint(
    nsINode* aBeginNode, PRInt32 aBeginOffset,
    nsINode* aEndNode, PRInt32 aEndOffset,
    PRInt32 aStartSearchingHere)
{
  PRInt32 i;
  NS_ASSERTION(aStartSearchingHere >= 0 && aStartSearchingHere <= (PRInt32)mRanges.Length(),
               "Input searching seed is not in range.");

  // search backwards for a begin match
  for (i = aStartSearchingHere; i >= 0 && i < (PRInt32)mRanges.Length(); i --) {
    if (RangeMatchesBeginPoint(mRanges[i].mRange, aBeginNode, aBeginOffset)) {
      if (RangeMatchesEndPoint(mRanges[i].mRange, aEndNode, aEndOffset))
        return i;
    } else {
      // done with matches going backwards
      break;
    }
  }

  // search forwards for a begin match
  for (i = aStartSearchingHere + 1; i < (PRInt32)mRanges.Length(); i ++) {
    if (RangeMatchesBeginPoint(mRanges[i].mRange, aBeginNode, aBeginOffset)) {
      if (RangeMatchesEndPoint(mRanges[i].mRange, aEndNode, aEndOffset))
        return i;
    } else {
      // done with matches going forwards
      break;
    }
  }

  // match not found
  return -1;
}

//utility method to get the primary frame of node or use the offset to get frame of child node

#if 0
NS_IMETHODIMP
nsTypedSelection::GetPrimaryFrameForRangeEndpoint(nsIDOMNode *aNode, PRInt32 aOffset, PRBool aIsEndNode, nsIFrame **aReturnFrame)
{
  if (!aNode || !aReturnFrame || !mFrameSelection)
    return NS_ERROR_NULL_POINTER;
  
  if (aOffset < 0)
    return NS_ERROR_FAILURE;

  *aReturnFrame = 0;
  
  nsresult  result = NS_OK;
  
  nsCOMPtr<nsIDOMNode> node = aNode;

  if (!node)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(node, &result);

  if (NS_FAILED(result))
    return result;

  if (!content)
    return NS_ERROR_NULL_POINTER;
  
  if (content->IsNodeOfType(nsINode::eELEMENT))
  {
    if (aIsEndNode)
      aOffset--;

    if (aOffset >= 0)
    {
      nsIContent *child = content->GetChildAt(aOffset);
      if (!child) //out of bounds?
        return NS_ERROR_FAILURE;

      content = child; // releases the focusnode
    }
  }
  *aReturnFrame = mFrameSelection->GetShell()->GetPrimaryFrameFor(content);
  return NS_OK;
}
#endif


NS_IMETHODIMP
nsTypedSelection::GetPrimaryFrameForAnchorNode(nsIFrame **aReturnFrame)
{
  if (!aReturnFrame)
    return NS_ERROR_NULL_POINTER;
  
  PRInt32 frameOffset = 0;
  *aReturnFrame = 0;
  nsCOMPtr<nsIContent> content = do_QueryInterface(GetAnchorNode());
  if (content && mFrameSelection)
  {
    *aReturnFrame = mFrameSelection->
      GetFrameForNodeOffset(content, GetAnchorOffset(),
                            mFrameSelection->GetHint(), &frameOffset);
    if (*aReturnFrame)
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTypedSelection::GetPrimaryFrameForFocusNode(nsIFrame **aReturnFrame, PRInt32 *aOffsetUsed,
                                              PRBool aVisual)
{
  if (!aReturnFrame)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(GetFocusNode());
  if (!content || !mFrameSelection)
    return NS_ERROR_FAILURE;
  
  nsIPresShell *presShell = mFrameSelection->GetShell();

  PRInt32 frameOffset = 0;
  *aReturnFrame = 0;
  if (!aOffsetUsed)
    aOffsetUsed = &frameOffset;
    
  nsFrameSelection::HINT hint = mFrameSelection->GetHint();

  if (aVisual) {
    nsRefPtr<nsCaret> caret;
    nsresult result = presShell->GetCaret(getter_AddRefs(caret));
    if (NS_FAILED(result) || !caret)
      return NS_ERROR_FAILURE;
    
    PRUint8 caretBidiLevel = mFrameSelection->GetCaretBidiLevel();

    return caret->GetCaretFrameForNodeOffset(content, GetFocusOffset(),
      hint, caretBidiLevel, aReturnFrame, aOffsetUsed);
  }
  
  *aReturnFrame = mFrameSelection->
    GetFrameForNodeOffset(content, GetFocusOffset(),
                          hint, aOffsetUsed);
  if (!*aReturnFrame)
    return NS_ERROR_FAILURE;

  return NS_OK;
}



//select all content children of aContent
nsresult
nsTypedSelection::selectFrames(nsPresContext* aPresContext,
                               nsIContentIterator *aInnerIter,
                               nsIContent *aContent,
                               nsIPresShell *aPresShell,
                               PRBool aFlags)
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do
  nsresult result;
  if (!aInnerIter)
    return NS_ERROR_NULL_POINTER;
  result = aInnerIter->Init(aContent);
  nsIFrame *frame;
  if (NS_SUCCEEDED(result))
  {
    // First select frame of content passed in
    frame = mFrameSelection->GetShell()->GetPrimaryFrameFor(aContent);
    if (frame)
    {
      //NOTE: eSpreadDown is now IGNORED. Selected state is set only for given frame
      frame->SetSelected(aPresContext, nsnull, aFlags, eSpreadDown, mType);
      if (mFrameSelection->GetTableCellSelection())
      {
        nsITableCellLayout *tcl = do_QueryFrame(frame);
        if (tcl)
        {
          return NS_OK;
        }
      }
    }
    // Now iterated through the child frames and set them
    while (!aInnerIter->IsDone())
    {
      nsCOMPtr<nsIContent> innercontent =
        do_QueryInterface(aInnerIter->GetCurrentNode());

      frame = mFrameSelection->GetShell()->GetPrimaryFrameFor(innercontent);
      if (frame)
      {
        //NOTE: eSpreadDown is now IGNORED. Selected state is set only
        //for given frame

        //spread from here to hit all frames in flow
        frame->SetSelected(aPresContext, nsnull, aFlags, eSpreadDown, mType);
        nsRect frameRect = frame->GetRect();

        //if a rect is 0 height/width then try to notify next
        //available in flow of selection status.
        while (!frameRect.width || !frameRect.height)
        {
          //try to notify next in flow that its content is selected.
          frame = frame->GetNextInFlow();
          if (frame)
          {
            frameRect = frame->GetRect();
            frame->SetSelected(aPresContext, nsnull, aFlags, eSpreadDown, mType);
          }
          else
            break;
        }
        //if the frame is splittable and this frame is 0,0 then set
        //the next in flow frame to be selected also
      }

      aInnerIter->Next();
    }

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}



//the idea of this helper method is to select, deselect "top to bottom" traversing through the frames
nsresult
nsTypedSelection::selectFrames(nsPresContext* aPresContext, nsIRange *aRange, PRBool aFlags)
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  nsCOMPtr<nsIDOMRange> domRange = do_QueryInterface(aRange);
  if (!domRange || !aPresContext) 
    return NS_ERROR_NULL_POINTER;

  nsresult result;
  nsCOMPtr<nsIContentIterator> iter = do_CreateInstance(
                                              kCSubtreeIteratorCID,
                                              &result);
  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIContentIterator> inneriter = do_CreateInstance(
                                              kCContentIteratorCID,
                                              &result);

  if ((NS_SUCCEEDED(result)) && iter && inneriter)
  {
    nsIPresShell *presShell = aPresContext->GetPresShell();
    result = iter->Init(aRange);

    // loop through the content iterator for each content node
    // for each text node, call SetSelected on it:
    nsCOMPtr<nsIContent> content = do_QueryInterface(aRange->GetStartParent());

    // we must call first one explicitly
    if (!content)
      return NS_ERROR_UNEXPECTED;

    nsIFrame *frame;
    if (!content->IsNodeOfType(nsINode::eELEMENT))
    {
      frame = mFrameSelection->GetShell()->GetPrimaryFrameFor(content);
      if (frame)
        frame->SetSelected(aPresContext, domRange, aFlags, eSpreadDown, mType);//spread from here to hit all frames in flow
    }

    iter->First();

    while (!iter->IsDone())
    {
      content = do_QueryInterface(iter->GetCurrentNode());

      selectFrames(aPresContext, inneriter, content, presShell,aFlags);

      iter->Next();
    }

    //we must now do the last one  if it is not the same as the first
    if (aRange->GetEndParent() != aRange->GetStartParent())
    {
      content = do_QueryInterface(aRange->GetEndParent(), &result);
      if (NS_FAILED(result) || !content)
        return result;

      if (!content->IsNodeOfType(nsINode::eELEMENT))
      {
        frame = mFrameSelection->GetShell()->GetPrimaryFrameFor(content);
        if (frame)
           frame->SetSelected(aPresContext, domRange, aFlags, eSpreadDown, mType);//spread from here to hit all frames in flow
      }
    }
  }
  return result;
}

// nsTypedSelection::LookUpSelection
//
//    This function is called when a node wants to know where the selection is
//    over itself.
//
//    Usually, this is called when we already know there is a selection over
//    the node in question, and we only need to find the boundaries of it on
//    that node. This is when slowCheck is false--a strict test is not needed.
//    Other times, the caller has no idea, and wants us to test everything,
//    so we are supposed to determine whether there is a selection over the
//    node at all.
//
//    A previous version of this code used this flag to do less work when
//    inclusion was already known (slowCheck=false). However, our tree
//    structure allows us to quickly determine ranges overlapping the node,
//    so we just ignore the slowCheck flag and do the full test every time.
//
//    PERFORMANCE: a common case is that we are doing a fast check with exactly
//    one range in the selection. In this case, this function is slower than
//    brute force because of the overhead of checking the tree. We can optimize
//    this case to make it faster by doing the same thing the previous version
//    of this function did in the case of 1 range. This would also mean that
//    the aSlowCheck flag would have meaning again.

NS_IMETHODIMP
nsTypedSelection::LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset,
                                  PRInt32 aContentLength,
                                  SelectionDetails **aReturnDetails,
                                  SelectionType aType, PRBool aSlowCheck)
{
  nsresult rv;
  if (! aContent || ! aReturnDetails)
    return NS_ERROR_NULL_POINTER;

  // it is common to have no ranges, to optimize that
  if (mRanges.Length() == 0)
    return NS_OK;

  nsCOMArray<nsIRange> overlappingRanges;
  rv = GetRangesForIntervalCOMArray(aContent, aContentOffset,
                                    aContent, aContentOffset + aContentLength,
                                    PR_FALSE,
                                    &overlappingRanges);
  NS_ENSURE_SUCCESS(rv, rv);
  if (overlappingRanges.Count() == 0)
    return NS_OK;

  for (PRInt32 i = 0; i < overlappingRanges.Count(); i ++) {
    nsIRange* range = overlappingRanges[i];
    nsINode* startNode = range->GetStartParent();
    nsINode* endNode = range->GetEndParent();
    PRInt32 startOffset = range->StartOffset();
    PRInt32 endOffset = range->EndOffset();

    PRInt32 start = -1, end = -1;
    if (startNode == aContent && endNode == aContent) {
      if (startOffset < (aContentOffset + aContentLength)  &&
          endOffset > aContentOffset) {
        // this range is totally inside the requested content range
        start = PR_MAX(0, startOffset - aContentOffset);
        end = PR_MIN(aContentLength, endOffset - aContentOffset);
      }
      // otherwise, range is inside the requested node, but does not intersect
      // the requested content range, so ignore it
    } else if (startNode == aContent) {
      if (startOffset < (aContentOffset + aContentLength)) {
        // the beginning of the range is inside the requested node, but the
        // end is outside, select everything from there to the end
        start = PR_MAX(0, startOffset - aContentOffset);
        end = aContentLength;
      }
    } else if (endNode == aContent) {
      if (endOffset > aContentOffset) {
        // the end of the range is inside the requested node, but the beginning
        // is outside, select everything from the beginning to there
        start = 0;
        end = PR_MIN(aContentLength, endOffset - aContentOffset);
      }
    } else {
      // this range does not begin or end in the requested node, but since
      // GetRangesForInterval returned this range, we know it overlaps.
      // Therefore, this node is enclosed in the range, and we select all
      // of it.
      start = 0;
      end = aContentLength;
    }
    if (start < 0)
      continue; // the ranges do not overlap the input range

    SelectionDetails* details = new SelectionDetails;
    if (! details)
      return NS_ERROR_OUT_OF_MEMORY;

    details->mNext = *aReturnDetails;
    details->mStart = start;
    details->mEnd = end;
    details->mType = aType;
    *aReturnDetails = details;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::Repaint(nsPresContext* aPresContext)
{
  PRInt32 arrCount = (PRInt32)mRanges.Length();

  if (arrCount < 1)
    return NS_OK;

  PRInt32 i;
  
  for (i = 0; i < arrCount; i++)
  {
    nsresult rv = selectFrames(aPresContext, mRanges[i].mRange, PR_TRUE);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::GetCanCacheFrameOffset(PRBool *aCanCacheFrameOffset)
{ 
  NS_ENSURE_ARG_POINTER(aCanCacheFrameOffset);

  if (mCachedOffsetForFrame)
    *aCanCacheFrameOffset = mCachedOffsetForFrame->mCanCacheFrameOffset;
  else
    *aCanCacheFrameOffset = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP    
nsTypedSelection::SetCanCacheFrameOffset(PRBool aCanCacheFrameOffset)
{
  if (!mCachedOffsetForFrame) {
    mCachedOffsetForFrame = new CachedOffsetForFrame;
  }

  mCachedOffsetForFrame->mCanCacheFrameOffset = aCanCacheFrameOffset;

  // clean up cached frame when turn off cache
  // fix bug 207936
  if (!aCanCacheFrameOffset) {
    mCachedOffsetForFrame->mLastCaretFrame = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsTypedSelection::GetCachedFrameOffset(nsIFrame *aFrame, PRInt32 inOffset, nsPoint& aPoint)
{
  if (!mCachedOffsetForFrame) {
    mCachedOffsetForFrame = new CachedOffsetForFrame;
  }

  nsresult rv = NS_OK;
  if (mCachedOffsetForFrame->mCanCacheFrameOffset &&
      mCachedOffsetForFrame->mLastCaretFrame &&
      (aFrame == mCachedOffsetForFrame->mLastCaretFrame) &&
      (inOffset == mCachedOffsetForFrame->mLastContentOffset))
  {
     // get cached frame offset
     aPoint = mCachedOffsetForFrame->mCachedFrameOffset;
  } 
  else
  {
     // Recalculate frame offset and cache it. Don't cache a frame offset if
     // GetPointFromOffset fails, though.
     rv = GetPointFromOffset(aFrame, inOffset, &aPoint);
     if (NS_SUCCEEDED(rv) && mCachedOffsetForFrame->mCanCacheFrameOffset) {
       mCachedOffsetForFrame->mCachedFrameOffset = aPoint;
       mCachedOffsetForFrame->mLastCaretFrame = aFrame;
       mCachedOffsetForFrame->mLastContentOffset = inOffset; 
     }
  }

  return rv;
}

NS_IMETHODIMP
nsTypedSelection::GetFrameSelection(nsFrameSelection **aFrameSelection) {
  NS_ENSURE_ARG_POINTER(aFrameSelection);
  *aFrameSelection = mFrameSelection;
  NS_IF_ADDREF(*aFrameSelection);
  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::SetAncestorLimiter(nsIContent *aContent)
{
  if (mFrameSelection)
    mFrameSelection->SetAncestorLimiter(aContent);
  return NS_OK;
}

nsresult
nsTypedSelection::StartAutoScrollTimer(nsPresContext *aPresContext,
                                       nsIView *aView,
                                       nsPoint& aPoint,
                                       PRUint32 aDelay)
{
  NS_PRECONDITION(aView, "Need a view");

  nsresult result;
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  if (!mAutoScrollTimer)
  {
    mAutoScrollTimer = new nsAutoScrollTimer();

    if (!mAutoScrollTimer)
      return NS_ERROR_OUT_OF_MEMORY;

    result = mAutoScrollTimer->Init(mFrameSelection, this);

    if (NS_FAILED(result))
      return result;
  }

  result = mAutoScrollTimer->SetDelay(aDelay);

  if (NS_FAILED(result))
    return result;

  return DoAutoScrollView(aPresContext, aView, aPoint, PR_TRUE);
}

nsresult
nsTypedSelection::StopAutoScrollTimer()
{
  if (mAutoScrollTimer)
    return mAutoScrollTimer->Stop();

  return NS_OK; 
}

nsresult
nsTypedSelection::GetViewAncestorOffset(nsIView *aView, nsIView *aAncestorView, nscoord *aXOffset, nscoord *aYOffset)
{
  // Note: A NULL aAncestorView pointer means that the caller wants
  //       the view's global offset.

  if (!aView || !aXOffset || !aYOffset)
    return NS_ERROR_FAILURE;

  nsPoint offset = aView->GetOffsetTo(aAncestorView);

  *aXOffset = offset.x;
  *aYOffset = offset.y;

  return NS_OK;
}

nsresult
nsTypedSelection::ScrollPointIntoClipView(nsPresContext *aPresContext, nsIView *aView, nsPoint& aPoint, PRBool *aDidScroll)
{
  nsresult result;

  if (!aPresContext || !aView || !aDidScroll)
    return NS_ERROR_NULL_POINTER;

  *aDidScroll = PR_FALSE;

  //
  // Get aView's scrollable view.
  //

  nsIScrollableView *scrollableView =
    nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eEither);

  if (!scrollableView)
    return NS_OK; // Nothing to do!

  //
  // Get the view that is being scrolled.
  //

  nsIView *scrolledView = 0;

  result = scrollableView->GetScrolledView(scrolledView);
  
  //
  // Now walk up aView's hierarchy, this time keeping track of
  // the view offsets until you hit the scrolledView.
  //

  nsPoint viewOffset(0,0);

  result = GetViewAncestorOffset(aView, scrolledView, &viewOffset.x, &viewOffset.y);

  if (NS_FAILED(result))
    return result;

  //
  // See if aPoint is outside the clip view's boundaries.
  // If it is, scroll the view till it is inside the visible area!
  //

  nsRect bounds = scrollableView->View()->GetBounds();

  result = scrollableView->GetScrollPosition(bounds.x,bounds.y);

  if (NS_FAILED(result))
    return result;

  //
  // Calculate the amount we would have to scroll in
  // the vertical and horizontal directions to get the point
  // within the clip area.
  //

  nscoord dx = 0, dy = 0;

  nsPresContext::ScrollbarStyles ss =
    nsLayoutUtils::ScrollbarStylesOfView(scrollableView);

  if (ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
    nscoord e = aPoint.x + viewOffset.x;
  
    nscoord x1 = bounds.x;
    nscoord x2 = bounds.x + bounds.width;

    if (e < x1)
      dx = e - x1;
    else if (e > x2)
      dx = e - x2;
  }

  if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN) {
    nscoord e = aPoint.y + viewOffset.y;

    nscoord y1 = bounds.y;
    nscoord y2 = bounds.y + bounds.height;

    if (e < y1)
      dy = e - y1;
    else if (e > y2)
      dy = e - y2;
  }

  //
  // Now scroll the view if necessary.
  //

  if (dx != 0 || dy != 0)
  {
    nsCOMPtr<nsIPresShell> presShell = aPresContext->GetPresShell();
    NS_ENSURE_STATE(presShell);

    nsWeakView weakView = scrollableView->View();

    // Make sure latest bits are available before we scroll them. This flushes
    // pending notifications and thus might destroy stuff (bug 421839).
    // We need to hold a strong ref on the view manager to keep it alive.
    nsCOMPtr<nsIViewManager> viewManager = presShell->GetViewManager();
    viewManager->Composite();

    if (!weakView.IsAlive()) {
      return NS_ERROR_NULL_POINTER;
    }

    if (presShell->IsDestroying()) {
      return NS_ERROR_NULL_POINTER;
    }

    result = scrollableView->ScrollTo(bounds.x + dx, bounds.y + dy, 0);

    if (NS_FAILED(result))
      return result;

    nsPoint newPos;

    result = scrollableView->GetScrollPosition(newPos.x, newPos.y);

    if (NS_FAILED(result))
      return result;

    *aDidScroll = (bounds.x != newPos.x || bounds.y != newPos.y);
  }

  return result;
}

nsresult
nsTypedSelection::ScrollPointIntoView(nsPresContext *aPresContext, nsIView *aView, nsPoint& aPoint, PRBool aScrollParentViews, PRBool *aDidScroll)
{
  if (!aPresContext || !aView || !aDidScroll)
    return NS_ERROR_NULL_POINTER;

  nsresult result;

  *aDidScroll = PR_FALSE;

  //
  // Calculate the global offset of the view.
  //

  nsPoint globalOffset;

  result = GetViewAncestorOffset(aView, nsnull, &globalOffset.x, &globalOffset.y);

  if (NS_FAILED(result))
    return result;

  //
  // Convert aPoint into global coordinates so it is easier to map
  // into other views.
  //

  nsPoint globalPoint = aPoint + globalOffset;

  //
  // Scroll the point into the visible rect of the closest
  // scrollable view.
  //
  result = ScrollPointIntoClipView(aPresContext, aView, aPoint, aDidScroll);

  if (NS_FAILED(result))
    return result;

  //
  // Now scroll the parent scrollable views.
  //

  if (aScrollParentViews)
  {
    //
    // Find aView's parent scrollable view.
    //

    nsIScrollableView *scrollableView =
      nsLayoutUtils::GetNearestScrollingView(aView, nsLayoutUtils::eEither);

    if (scrollableView)
    {
      //
      // Convert scrollableView to nsIView.
      //

      nsIView *scrolledView = 0;
      nsIView *view = scrollableView->View();

      if (view)
      {
        //
        // Now get the scrollableView's parent, then search for it's
        // closest scrollable view.
        //

        view = view->GetParent();

        while (view)
        {
          scrollableView =
            nsLayoutUtils::GetNearestScrollingView(view,
                                                   nsLayoutUtils::eEither);

          if (!scrollableView)
            break;

          scrolledView = 0;
          result = scrollableView->GetScrolledView(scrolledView);
          
          if (NS_FAILED(result))
            return result;

          //
          // Map the global point into this scrolledView's coordinate space.
          //

          result = GetViewAncestorOffset(scrolledView, nsnull, &globalOffset.x, &globalOffset.y);

          if (NS_FAILED(result))
            return result;

          nsPoint newPoint = globalPoint - globalOffset;

          //
          // Scroll the point into the visible rect of the scrolled view.
          //

          PRBool parentDidScroll = PR_FALSE;

          result = ScrollPointIntoClipView(aPresContext, scrolledView, newPoint, &parentDidScroll);

          if (NS_FAILED(result))
            return result;

          *aDidScroll = *aDidScroll || parentDidScroll;

          //
          // Now get the parent of this scrollable view so we
          // can scroll the next parent view.
          //

          view = scrollableView->View()->GetParent();
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsTypedSelection::DoAutoScrollView(nsPresContext *aPresContext,
                                   nsIView *aView,
                                   nsPoint& aPoint,
                                   PRBool aScrollParentViews)
{
  if (!aPresContext || !aView)
    return NS_ERROR_NULL_POINTER;

  nsresult result;

  if (mAutoScrollTimer)
    result = mAutoScrollTimer->Stop();

  //
  // Calculate the global offset of the view.
  //

  nsPoint globalOffset;
  result = GetViewAncestorOffset(aView, nsnull, &globalOffset.x, &globalOffset.y);
  NS_ENSURE_SUCCESS(result, result);

  //
  // Convert aPoint into global coordinates so we can get back
  // to the same point after all the parent views have scrolled.
  //
  nsPoint globalPoint = aPoint + globalOffset;
  //
  // Now scroll aPoint into view.
  //

  PRBool didScroll = PR_FALSE;

  result = ScrollPointIntoView(aPresContext, aView, aPoint, aScrollParentViews, &didScroll);
  NS_ENSURE_SUCCESS(result, result);

  //
  // Start the AutoScroll timer if necessary.
  //

  if (didScroll && mAutoScrollTimer)
  {
    //
    // Map the globalPoint back into aView's coordinate system. We
    // have to get the globalOffsets again because aView's
    // window and its parents may have changed their offsets.
    //
    result = GetViewAncestorOffset(aView, nsnull, &globalOffset.x, &globalOffset.y);
    NS_ENSURE_SUCCESS(result, result);

    nsPoint svPoint = globalPoint - globalOffset;
    mAutoScrollTimer->Start(aPresContext, aView, svPoint);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::GetEnumerator(nsIEnumerator **aIterator)
{
  nsresult status = NS_ERROR_OUT_OF_MEMORY;
  nsSelectionIterator *iterator =  new nsSelectionIterator(this);
  if ( iterator && NS_FAILED(status = CallQueryInterface(iterator, aIterator)) )
    delete iterator;
  return status;
}



/** RemoveAllRanges zeroes the selection
 */
NS_IMETHODIMP
nsTypedSelection::RemoveAllRanges()
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do
  nsCOMPtr<nsPresContext>  presContext;
  GetPresContext(getter_AddRefs(presContext));


  nsresult  result = Clear(presContext);
  if (NS_FAILED(result))
    return result;
  
  // Turn off signal for table selection
  mFrameSelection->ClearTableCellSelection();

  return mFrameSelection->NotifySelectionListeners(GetType());
  // Also need to notify the frames!
  // PresShell::CharacterDataChanged should do that on DocumentChanged
}

/** AddRange adds the specified range to the selection
 *  @param aRange is the range to be added
 */
NS_IMETHODIMP
nsTypedSelection::AddRange(nsIDOMRange* aRange)
{
  nsCOMPtr<nsIRange> range = do_QueryInterface(aRange);
  return AddRange(range);
}

nsresult
nsTypedSelection::AddRange(nsIRange* aRange)
{
  if (!aRange) return NS_ERROR_NULL_POINTER;

  // This inserts a table cell range in proper document order
  // and returns NS_OK if range doesn't contain just one table cell
  PRBool didAddRange;
  PRInt32 rangeIndex;
  nsresult result = addTableCellRange(aRange, &didAddRange, &rangeIndex);
  if (NS_FAILED(result)) return result;

  if (!didAddRange)
  {
    result = AddItem(aRange, &rangeIndex);
    if (NS_FAILED(result)) return result;
  }

  NS_ASSERTION(rangeIndex >= 0, "Range index not returned");
  setAnchorFocusRange(rangeIndex);
  
  // Make sure the caret appears on the next line, if at a newline
  if (mType == nsISelectionController::SELECTION_NORMAL)
    SetInterlinePosition(PR_TRUE);

  nsCOMPtr<nsPresContext>  presContext;
  GetPresContext(getter_AddRefs(presContext));
  selectFrames(presContext, aRange, PR_TRUE);        

  //ScrollIntoView(); this should not happen automatically
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  return mFrameSelection->NotifySelectionListeners(GetType());
}

// nsTypedSelection::RemoveRange
//
//    Removes the given range from the selection. The tricky part is updating
//    the flags on the frames that indicate whether they have a selection or
//    not. There could be several selection ranges on the frame, and clearing
//    the bit would cause the selection to not be drawn, even when there is
//    another range on the frame (bug 346185).
//
//    We therefore find any ranges that intersect the same nodes as the range
//    being removed, and cause them to set the selected bits back on their
//    selected frames after we've cleared the bit from ours.

NS_IMETHODIMP
nsTypedSelection::RemoveRange(nsIDOMRange* aRange)
{
  nsCOMPtr<nsIRange> range = do_QueryInterface(aRange);
  return RemoveRange(range);
}

nsresult
nsTypedSelection::RemoveRange(nsIRange* aRange)
{
  if (!aRange)
    return NS_ERROR_INVALID_ARG;
  nsresult rv = RemoveItem(aRange);
  if (NS_FAILED(rv))
    return rv;

  nsINode* beginNode = aRange->GetStartParent();
  nsINode* endNode = aRange->GetEndParent();
  
  // find out the length of the end node, so we can select all of it
  PRInt32 beginOffset, endOffset;
  if (endNode->IsNodeOfType(nsINode::eTEXT)) {
    // Get the length of the text. We can't just use the offset because
    // another range could be touching this text node but not intersect our
    // range.
    beginOffset = 0;
    endOffset = static_cast<nsIContent*>(endNode)->TextLength();
  } else {
    // For non-text nodes, the given offsets should be sufficient.
    beginOffset = aRange->StartOffset();
    endOffset = aRange->EndOffset();
  }

  // clear the selected bit from the removed range's frames
  nsCOMPtr<nsPresContext>  presContext;
  GetPresContext(getter_AddRefs(presContext));
  selectFrames(presContext, aRange, PR_FALSE);

  // add back the selected bit for each range touching our nodes
  nsCOMArray<nsIRange> affectedRanges;
  rv = GetRangesForIntervalCOMArray(beginNode, beginOffset,
                                    endNode, endOffset,
                                    PR_TRUE, &affectedRanges);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRInt32 i = 0; i < affectedRanges.Count(); i ++) {
    selectFrames(presContext, affectedRanges[i], PR_TRUE);
  }

  PRInt32 cnt = mRanges.Length();
  if (aRange == mAnchorFocusRange) {
    // Reset anchor to LAST range or clear it if there are no ranges.
    setAnchorFocusRange(cnt - 1);

    // When the selection is user-created it makes sense to scroll the range
    // into view. The spell-check selection, however, is created and destroyed
    // in the background. We don't want to scroll in this case or the view
    // might appear to be moving randomly (bug 337871).
    if (mType != nsISelectionController::SELECTION_SPELLCHECK && cnt > 0)
      ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION, PR_FALSE,
                     PR_FALSE);
  }

  if (!mFrameSelection)
    return NS_OK;//nothing to do
  return mFrameSelection->NotifySelectionListeners(GetType());
}



/*
 * Collapse sets the whole selection to be one point.
 */
NS_IMETHODIMP
nsTypedSelection::Collapse(nsIDOMNode* aParentNode, PRInt32 aOffset)
{
  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  return Collapse(parentNode, aOffset);
}

nsresult
nsTypedSelection::Collapse(nsINode* aParentNode, PRInt32 aOffset)
{
  if (!aParentNode)
    return NS_ERROR_INVALID_ARG;
  if (!mFrameSelection)
    return NS_ERROR_NOT_INITIALIZED; // Can't do selection
  mFrameSelection->InvalidateDesiredX();
  if (!IsValidSelectionPoint(mFrameSelection, aParentNode))
    return NS_ERROR_FAILURE;
  nsresult result;
  // Delete all of the current ranges
  nsCOMPtr<nsPresContext>  presContext;
  GetPresContext(getter_AddRefs(presContext));
  Clear(presContext);

  // Turn off signal for table selection
  mFrameSelection->ClearTableCellSelection();

  nsCOMPtr<nsIRange> range = new nsRange();
  if (! range){
    NS_ASSERTION(PR_FALSE,"Couldn't make a range - nsFrameSelection::Collapse");
    return NS_ERROR_UNEXPECTED;
  }
  result = range->SetEnd(aParentNode, aOffset);
  if (NS_FAILED(result))
    return result;
  result = range->SetStart(aParentNode, aOffset);
  if (NS_FAILED(result))
    return result;

#ifdef DEBUG_SELECTION
  if (aParentNode)
  {
    nsCOMPtr<nsIContent>content;
    content = do_QueryInterface(aParentNode);
    if (!content)
      return NS_ERROR_FAILURE;

    const char *tagString;
    content->Tag()->GetUTF8String(&tagString);
    printf ("Sel. Collapse to %p %s %d\n", content.get(), tagString, aOffset);
  }
  else {
    printf ("Sel. Collapse set to null parent.\n");
  }
#endif


  result = AddItem(range);
  setAnchorFocusRange(0);
  selectFrames(presContext, range, PR_TRUE);
  if (NS_FAILED(result))
    return result;
  return mFrameSelection->NotifySelectionListeners(GetType());
}

/*
 * Sets the whole selection to be one point
 * at the start of the current selection
 */
NS_IMETHODIMP
nsTypedSelection::CollapseToStart()
{
  PRInt32 cnt;
  nsresult rv = GetRangeCount(&cnt);
  if (NS_FAILED(rv) || cnt <= 0)
    return NS_ERROR_FAILURE;

  // Get the first range
  nsIRange* firstRange = mRanges[0].mRange;
  if (!firstRange)
    return NS_ERROR_FAILURE;

  return Collapse(firstRange->GetStartParent(), firstRange->StartOffset());
}

/*
 * Sets the whole selection to be one point
 * at the end of the current selection
 */
NS_IMETHODIMP
nsTypedSelection::CollapseToEnd()
{
  PRInt32 cnt;
  nsresult rv = GetRangeCount(&cnt);
  if (NS_FAILED(rv) || cnt <= 0)
    return NS_ERROR_FAILURE;

  // Get the last range
  nsIRange* lastRange = mRanges[cnt-1].mRange;
  if (!lastRange)
    return NS_ERROR_FAILURE;

  return Collapse(lastRange->GetEndParent(), lastRange->EndOffset());
}

/*
 * IsCollapsed -- is the whole selection just one point, or unset?
 */
NS_IMETHODIMP
nsTypedSelection::GetIsCollapsed(PRBool* aIsCollapsed)
{
  if (!aIsCollapsed)
    return NS_ERROR_NULL_POINTER;

  PRInt32 cnt = (PRInt32)mRanges.Length();;
  if (cnt == 0)
  {
    *aIsCollapsed = PR_TRUE;
    return NS_OK;
  }
  
  if (cnt != 1)
  {
    *aIsCollapsed = PR_FALSE;
    return NS_OK;
  }
  
  *aIsCollapsed = mRanges[0].mRange->Collapsed();
  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::GetRangeCount(PRInt32* aRangeCount)
{
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");
  *aRangeCount = (PRInt32)mRanges.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::GetRangeAt(PRInt32 aIndex, nsIDOMRange** aReturn)
{
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");

  *aReturn = mRanges.SafeElementAt(aIndex, sEmptyData).mRange;
  if (!*aReturn) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ADDREF(*aReturn);

  return NS_OK;
}

nsIRange*
nsTypedSelection::GetRangeAt(PRInt32 aIndex)
{
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");
  return mRanges.SafeElementAt(aIndex, sEmptyData).mRange;
}

/*
utility function
*/
nsresult
nsTypedSelection::CopyRangeToAnchorFocus(nsIRange *aRange)
{
  // XXXbz could we just clone into mAnchorFocusRange, or do consumers
  // expect that pointer to not change across this call?
  NS_ENSURE_STATE(mAnchorFocusRange);
  
  nsINode* startNode = aRange->GetStartParent();
  nsINode* endNode = aRange->GetEndParent();
  PRInt32 startOffset = aRange->StartOffset();
  PRInt32 endOffset = aRange->EndOffset();;
  if (NS_FAILED(mAnchorFocusRange->SetStart(startNode,startOffset)))
  {
    // XXXbz what is this doing exactly?
    if (NS_FAILED(mAnchorFocusRange->SetEnd(endNode,endOffset)))
      return NS_ERROR_FAILURE;//???
    if (NS_FAILED(mAnchorFocusRange->SetStart(startNode,startOffset)))
      return NS_ERROR_FAILURE;//???
  }
  else if (NS_FAILED(mAnchorFocusRange->SetEnd(endNode,endOffset)))
          return NS_ERROR_FAILURE;//???
  return NS_OK;
}

void
nsTypedSelection::ReplaceAnchorFocusRange(nsIRange *aRange)
{
  nsCOMPtr<nsPresContext> presContext;
  GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    selectFrames(presContext, mAnchorFocusRange, PR_FALSE);
    CopyRangeToAnchorFocus(aRange);
    selectFrames(presContext, mAnchorFocusRange, PR_TRUE);
  }
}

/*
Notes which might come in handy for extend:

We can tell the direction of the selection by asking for the anchors selection
if the begin is less than the end then we know the selection is to the "right".
else it is a backwards selection.
a = anchor
1 = old cursor
2 = new cursor

  if (a <= 1 && 1 <=2)    a,1,2  or (a1,2)
  if (a < 2 && 1 > 2)     a,2,1
  if (1 < a && a <2)      1,a,2
  if (a > 2 && 2 >1)      1,2,a
  if (2 < a && a <1)      2,a,1
  if (a > 1 && 1 >2)      2,1,a
then execute
a  1  2 select from 1 to 2
a  2  1 deselect from 2 to 1
1  a  2 deselect from 1 to a select from a to 2
1  2  a deselect from 1 to 2
2  1  a = continue selection from 2 to 1
*/


/*
 * Extend extends the selection away from the anchor.
 * We don't need to know the direction, because we always change the focus.
 */
NS_IMETHODIMP
nsTypedSelection::Extend(nsIDOMNode* aParentNode, PRInt32 aOffset)
{
  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  return Extend(parentNode, aOffset);
}

nsresult
nsTypedSelection::Extend(nsINode* aParentNode, PRInt32 aOffset)
{
  if (!aParentNode)
    return NS_ERROR_INVALID_ARG;

  // First, find the range containing the old focus point:
  if (!mAnchorFocusRange)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mFrameSelection)
    return NS_ERROR_NOT_INITIALIZED; // Can't do selection

  nsresult res;
  if (!IsValidSelectionPoint(mFrameSelection, aParentNode))
    return NS_ERROR_FAILURE;

  //mFrameSelection->InvalidateDesiredX();
  nsCOMPtr<nsIRange> difRange = new nsRange();
  nsCOMPtr<nsIRange> range;

  nsINode* anchorNode = GetAnchorNode();
  nsINode* focusNode = GetFocusNode();
  PRInt32 anchorOffset = GetAnchorOffset();
  PRInt32 focusOffset = GetFocusOffset();

  if (focusNode == aParentNode && focusOffset == aOffset)
    return NS_ERROR_FAILURE;//same node nothing to do!

  res = mAnchorFocusRange->CloneRange(getter_AddRefs(range));
  if (NS_FAILED(res))
    return res;
  //range = mAnchorFocusRange;

  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
  PRInt32 startOffset = range->StartOffset();
  PRInt32 endOffset = range->EndOffset();;

  nsDirection dir = GetDirection();

  //compare anchor to old cursor.

  if (NS_FAILED(res))
    return res;
  PRInt32 result1 = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  focusNode, focusOffset);
  //compare old cursor to new cursor
  PRInt32 result2 = nsContentUtils::ComparePoints(focusNode, focusOffset,
                                                  aParentNode, aOffset);
  //compare anchor to new cursor
  PRInt32 result3 = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  aParentNode, aOffset);

  if (result2 == 0) //not selecting anywhere
    return NS_OK;

  nsCOMPtr<nsPresContext>  presContext;
  GetPresContext(getter_AddRefs(presContext));
  if ((result1 == 0 && result3 < 0) || (result1 <= 0 && result2 < 0)){//a1,2  a,1,2
    //select from 1 to 2 unless they are collapsed
    res = range->SetEnd(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;
    dir = eDirNext;
    res = difRange->SetEnd(range->GetEndParent(), range->EndOffset());
    res |= difRange->SetStart(focusNode, focusOffset);
    if (NS_FAILED(res))
      return res;
    selectFrames(presContext, difRange , PR_TRUE);
    res = CopyRangeToAnchorFocus(range);
    if (NS_FAILED(res))
      return res;
  }
  else if (result1 == 0 && result3 > 0){//2, a1
    //select from 2 to 1a
    dir = eDirPrevious;
    res = range->SetStart(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;
    selectFrames(presContext, range, PR_TRUE);
    res = CopyRangeToAnchorFocus(range);
    if (NS_FAILED(res))
      return res;
  }
  else if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
    //deselect from 2 to 1
    res = difRange->SetEnd(focusNode, focusOffset);
    res |= difRange->SetStart(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;

    res = range->SetEnd(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;
    res = CopyRangeToAnchorFocus(range);
    if (NS_FAILED(res))
      return res;
    RemoveItem(mAnchorFocusRange);
    selectFrames(presContext, difRange, PR_FALSE); // deselect now
    AddItem(mAnchorFocusRange);
    difRange->SetEnd(range->GetEndParent(), range->EndOffset());
    selectFrames(presContext, difRange, PR_TRUE); // must reselect last node maybe more
  }
  else if (result1 >= 0 && result3 <= 0) {//1,a,2 or 1a,2 or 1,a2 or 1a2
    if (GetDirection() == eDirPrevious){
      res = range->SetStart(endNode, endOffset);
      if (NS_FAILED(res))
        return res;
    }
    dir = eDirNext;
    res = range->SetEnd(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;
    if (focusNode != anchorNode || focusOffset != anchorOffset) {//if collapsed diff dont do anything
      res = difRange->SetStart(focusNode, focusOffset);
      res |= difRange->SetEnd(anchorNode, anchorOffset);
      if (NS_FAILED(res))
        return res;
      res = CopyRangeToAnchorFocus(range);
      if (NS_FAILED(res))
        return res;
      //deselect from 1 to a
      RemoveItem(mAnchorFocusRange);
      selectFrames(presContext, difRange , PR_FALSE);
      AddItem(mAnchorFocusRange);
    }
    else
    {
      res = CopyRangeToAnchorFocus(range);
      if (NS_FAILED(res))
        return res;
    }
    //select from a to 2
    selectFrames(presContext, range , PR_TRUE);
  }
  else if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
    //deselect from 1 to 2
    res = difRange->SetEnd(aParentNode, aOffset);
    res |= difRange->SetStart(focusNode, focusOffset);
    if (NS_FAILED(res))
      return res;
    dir = eDirPrevious;
    res = range->SetStart(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;

    res = CopyRangeToAnchorFocus(range);
    if (NS_FAILED(res))
      return res;
    RemoveItem(mAnchorFocusRange);
    selectFrames(presContext, difRange , PR_FALSE);
    AddItem(mAnchorFocusRange);
    difRange->SetStart(range->GetStartParent(), range->StartOffset());
    selectFrames(presContext, difRange, PR_TRUE);//must reselect last node
  }
  else if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
    if (GetDirection() == eDirNext){
      range->SetEnd(startNode, startOffset);
    }
    dir = eDirPrevious;
    res = range->SetStart(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;
    //deselect from a to 1
    if (focusNode != anchorNode || focusOffset!= anchorOffset) {//if collapsed diff dont do anything
      res = difRange->SetStart(anchorNode, anchorOffset);
      res |= difRange->SetEnd(focusNode, focusOffset);
      res |= CopyRangeToAnchorFocus(range);
      if (NS_FAILED(res))
        return res;
      RemoveItem(mAnchorFocusRange);
      selectFrames(presContext, difRange, 0);
      AddItem(mAnchorFocusRange);
    }
    else
    {
      res = CopyRangeToAnchorFocus(range);
      if (NS_FAILED(res))
        return res;
    }
    //select from 2 to a
    selectFrames(presContext, range , PR_TRUE);
  }
  else if (result2 >= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
    //select from 2 to 1
    res = range->SetStart(aParentNode, aOffset);
    if (NS_FAILED(res))
      return res;
    dir = eDirPrevious;
    res = difRange->SetEnd(focusNode, focusOffset);
    res |= difRange->SetStart(range->GetStartParent(), range->StartOffset());
    if (NS_FAILED(res))
      return res;

    selectFrames(presContext, difRange, PR_TRUE);
    res = CopyRangeToAnchorFocus(range);
    if (NS_FAILED(res))
      return res;
  }

  DEBUG_OUT_RANGE(range);
#ifdef DEBUG_SELECTION
  if (eDirNext == mDirection)
    printf("    direction = 1  LEFT TO RIGHT\n");
  else
    printf("    direction = 0  RIGHT TO LEFT\n");
#endif
  SetDirection(dir);
#ifdef DEBUG_SELECTION
  if (aParentNode)
  {
    nsCOMPtr<nsIContent>content;
    content = do_QueryInterface(aParentNode);

    const char *tagString;
    content->Tag()->GetUTF8String(&tagString);
    printf ("Sel. Extend to %p %s %d\n", content.get(), tagString, aOffset);
  }
  else {
    printf ("Sel. Extend set to null parent.\n");
  }
#endif
  return mFrameSelection->NotifySelectionListeners(GetType());
}

static nsresult
GetChildOffset(nsIDOMNode *aChild, nsIDOMNode *aParent, PRInt32 &aOffset)
{
  NS_ASSERTION((aChild && aParent), "bad args");
  nsCOMPtr<nsIContent> content = do_QueryInterface(aParent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(aChild);

  if (!cChild || !content)
    return NS_ERROR_NULL_POINTER;

  aOffset = content->IndexOf(cChild);

  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::SelectAllChildren(nsIDOMNode* aParentNode)
{
  NS_ENSURE_ARG_POINTER(aParentNode);
  
  if (mFrameSelection) 
  {
    mFrameSelection->PostReason(nsISelectionListener::SELECTALL_REASON);
  }
  nsresult result = Collapse(aParentNode, 0);
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode>lastChild;
    result = aParentNode->GetLastChild(getter_AddRefs(lastChild));
    if ((NS_SUCCEEDED(result)) && lastChild)
    {
      PRInt32 numBodyChildren=0;
      GetChildOffset(lastChild, aParentNode, numBodyChildren);
      if (mFrameSelection) 
      {
        mFrameSelection->PostReason(nsISelectionListener::SELECTALL_REASON);
      }
      result = Extend(aParentNode, numBodyChildren+1);
    }
  }
  return result;
}

NS_IMETHODIMP
nsTypedSelection::ContainsNode(nsIDOMNode* aNode, PRBool aAllowPartial,
                               PRBool* aYes)
{
  nsresult rv;
  if (!aYes)
    return NS_ERROR_NULL_POINTER;
  NS_ASSERTION(ValidateRanges(), "Ranges out of sync");
  *aYes = PR_FALSE;

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (mRanges.Length() == 0 || !node)
    return NS_OK;

  // XXXbz this duplicates the GetNodeLength code in nsRange.cpp
  PRUint32 nodeLength;
  PRBool isData = node->IsNodeOfType(nsINode::eDATA_NODE);
  if (isData) {
    nodeLength = static_cast<nsIContent*>(node.get())->TextLength();
  } else {
    nodeLength = node->GetChildCount();
  }

  nsCOMArray<nsIRange> overlappingRanges;
  rv = GetRangesForIntervalCOMArray(node, 0, node, nodeLength,
                                    PR_FALSE, &overlappingRanges);
  NS_ENSURE_SUCCESS(rv, rv);
  if (overlappingRanges.Count() == 0)
    return NS_OK; // no ranges overlap
  
  // if the caller said partial intersections are OK, we're done
  if (aAllowPartial) {
    *aYes = PR_TRUE;
    return NS_OK;
  }

  // text nodes always count as inside
  if (isData) {
    *aYes = PR_TRUE;
    return NS_OK;
  }

  // The caller wants to know if the node is entirely within the given range,
  // so we have to check all intersecting ranges.
  for (PRInt32 i = 0; i < overlappingRanges.Count(); i ++) {
    PRBool nodeStartsBeforeRange, nodeEndsAfterRange;
    if (NS_SUCCEEDED(nsRange::CompareNodeToRange(node, overlappingRanges[i],
                                                 &nodeStartsBeforeRange,
                                                 &nodeEndsAfterRange))) {
      if (!nodeStartsBeforeRange && !nodeEndsAfterRange) {
        *aYes = PR_TRUE;
        return NS_OK;
      }
    }
  }
  return NS_OK;
}


nsresult
nsTypedSelection::GetPresContext(nsPresContext **aPresContext)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;//nothing to do
  nsIPresShell *shell = mFrameSelection->GetShell();

  if (!shell)
    return NS_ERROR_NULL_POINTER;

  NS_IF_ADDREF(*aPresContext = shell->GetPresContext());
  return NS_OK;
}

nsresult
nsTypedSelection::GetPresShell(nsIPresShell **aPresShell)
{
  if (mPresShellWeak)
  {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
    if (presShell)
      NS_ADDREF(*aPresShell = presShell);
    return NS_OK;
  }
  nsresult rv = NS_OK;
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;//nothing to do

  nsIPresShell *shell = mFrameSelection->GetShell();

  mPresShellWeak = do_GetWeakReference(shell);    // the presshell owns us, so no addref
  if (mPresShellWeak)
    NS_ADDREF(*aPresShell = shell);
  return rv;
}

nsresult
nsTypedSelection::GetRootScrollableView(nsIScrollableView **aScrollableView)
{
  //
  // NOTE: This method returns a NON-AddRef'd pointer
  //       to the scrollable view!
  //
  NS_ENSURE_ARG_POINTER(aScrollableView);

  if (!mFrameSelection)
    return NS_ERROR_FAILURE;//nothing to do

  nsIScrollableView *scrollView = mFrameSelection->GetScrollableView();
  if (!scrollView)
  {
    nsCOMPtr<nsIPresShell> presShell;

    nsresult rv = GetPresShell(getter_AddRefs(presShell));

    if (NS_FAILED(rv))
      return rv;

    if (!presShell)
      return NS_ERROR_NULL_POINTER;

    nsIViewManager* viewManager = presShell->GetViewManager();

    if (!viewManager)
      return NS_ERROR_NULL_POINTER;

    //
    // nsIViewManager::GetRootScrollableView() does not
    // AddRef the pointer it returns.
    //
    return viewManager->GetRootScrollableView(aScrollableView);
  }

  *aScrollableView = scrollView;
  return NS_OK;
}

nsresult
nsTypedSelection::GetFrameToScrolledViewOffsets(nsIScrollableView *aScrollableView, nsIFrame *aFrame, nscoord *aX, nscoord *aY)
{
  nsresult rv = NS_OK;
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;//nothing to do

  if (!aScrollableView || !aFrame || !aX || !aY) {
    return NS_ERROR_NULL_POINTER;
  }

  *aX = 0;
  *aY = 0;

  nsIView*  scrolledView;
  nsPoint   offset;
  nsIView*  closestView;
          
  // Determine the offset from aFrame to the scrolled view. We do that by
  // getting the offset from its closest view and then walking up
  aScrollableView->GetScrolledView(scrolledView);
  nsIPresShell *shell = mFrameSelection->GetShell();

  if (!shell)
    return NS_ERROR_NULL_POINTER;

  aFrame->GetOffsetFromView(offset, &closestView);

  // XXX Deal with the case where there is a scrolled element, e.g., a
  // DIV in the middle...
  offset += closestView->GetOffsetTo(scrolledView);

  *aX = offset.x;
  *aY = offset.y;

  return rv;
}

nsresult
nsTypedSelection::GetPointFromOffset(nsIFrame *aFrame, PRInt32 aContentOffset, nsPoint *aPoint)
{
  nsresult rv = NS_OK;
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;//nothing to do
  if (!aFrame || !aPoint)
    return NS_ERROR_NULL_POINTER;

  aPoint->x = 0;
  aPoint->y = 0;

  //
  // Now get the closest view with a widget so we can create
  // a rendering context.
  //

  nsIWidget* widget = nsnull;
  nsIView *closestView = nsnull;
  nsPoint offset(0, 0);

  rv = aFrame->GetOffsetFromView(offset, &closestView);

  while (!widget && closestView)
  {
    widget = closestView->GetWidget();

    if (!widget)
    {
      closestView = closestView->GetParent();
    }
  }

  if (!closestView)
    return NS_ERROR_FAILURE;

  //
  // Now get the point and return!
  //

  rv = aFrame->GetPointFromOffset(aContentOffset, aPoint);

  return rv;
}

nsresult
nsTypedSelection::GetSelectionRegionRectAndScrollableView(SelectionRegion aRegion, nsRect *aRect, nsIScrollableView **aScrollableView)
{
  if (!mFrameSelection)
    return NS_ERROR_FAILURE;  // nothing to do

  NS_ENSURE_TRUE(aRect && aScrollableView, NS_ERROR_NULL_POINTER);

  aRect->SetRect(0, 0, 0, 0);
  *aScrollableView = nsnull;

  nsINode    *node       = nsnull;
  PRInt32     nodeOffset = 0;
  nsIFrame   *frame      = nsnull;

  switch (aRegion) {
    case nsISelectionController::SELECTION_ANCHOR_REGION:
      node       = GetAnchorNode();
      nodeOffset = GetAnchorOffset();
      break;
    case nsISelectionController::SELECTION_FOCUS_REGION:
      node       = GetFocusNode();
      nodeOffset = GetFocusOffset();
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  if (!node)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  NS_ENSURE_TRUE(content.get(), NS_ERROR_FAILURE);
  PRInt32 frameOffset = 0;
  frame = mFrameSelection->GetFrameForNodeOffset(content, nodeOffset,
                                                 mFrameSelection->GetHint(),
                                                 &frameOffset);
  if (!frame)
    return NS_ERROR_FAILURE;

  // Get the frame's nearest scrollable view.
  nsIFrame* parentWithView = frame->GetAncestorWithView();
  if (!parentWithView)
    return NS_ERROR_FAILURE;
  nsIView* view = parentWithView->GetView();
  *aScrollableView =
    nsLayoutUtils::GetNearestScrollingView(view, nsLayoutUtils::eEither);
  if (!*aScrollableView)
    return NS_OK;

  // Figure out what node type we have, then get the
  // appropriate rect for it's nodeOffset.
  PRBool isText = node->IsNodeOfType(nsINode::eTEXT);

  nsPoint pt(0, 0);
  if (isText) {
    nsIFrame* childFrame = nsnull;
    frameOffset = 0;
    nsresult rv =
      frame->GetChildFrameContainingOffset(nodeOffset,
                                           mFrameSelection->GetHint(),
                                           &frameOffset, &childFrame);
    if (NS_FAILED(rv))
      return rv;
    if (!childFrame)
      return NS_ERROR_NULL_POINTER;

    frame = childFrame;

    // Get the x coordinate of the offset into the text frame.
    rv = GetCachedFrameOffset(frame, nodeOffset, pt);
    if (NS_FAILED(rv))
      return rv;
  }

  // Get the frame's rect in scroll view coordinates.
  *aRect = frame->GetRect();
  nsresult rv = GetFrameToScrolledViewOffsets(*aScrollableView, frame,
                                              &aRect->x, &aRect->y);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isText) {
    aRect->x += pt.x;
  }
  else if (mFrameSelection->GetHint() == nsFrameSelection::HINTLEFT) {
    // It's the frame's right edge we're interested in.
    aRect->x += aRect->width;
  }

  nsRect clipRect = (*aScrollableView)->View()->GetBounds();
  rv = (*aScrollableView)->GetScrollPosition(clipRect.x, clipRect.y);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the point we are interested in is outside the clip region, we aim
  // to over-scroll it by a quarter of the clip's width.
  PRInt32 pad = clipRect.width / 4;

  if (pad == 0)
    pad = 3; // Arbitrary

  if (aRect->x >= clipRect.XMost()) {
    aRect->width = pad;
  }
  else if (aRect->x <= clipRect.x) {
    aRect->x -= pad;
    aRect->width = pad;
  }
  else {
    aRect->width = 60; // Arbitrary
  }

  return rv;
}

static void
ClampPointInsideRect(nsPoint& aPoint, const nsRect& aRect)
{
  if (aPoint.x < aRect.x)
    aPoint.x = aRect.x;
  if (aPoint.x > aRect.XMost())
    aPoint.x = aRect.XMost();
  if (aPoint.y < aRect.y)
    aPoint.y = aRect.y;
  if (aPoint.y > aRect.YMost())
    aPoint.y = aRect.YMost();
}

nsresult
nsTypedSelection::ScrollRectIntoView(nsIScrollableView *aScrollableView,
                              nsRect& aRect,
                              PRIntn  aVPercent, 
                              PRIntn  aHPercent,
                              PRBool aScrollParentViews)
{
  nsresult rv = NS_OK;
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  if (!aScrollableView)
    return NS_ERROR_NULL_POINTER;

  // Determine the visible rect in the scrolled view's coordinate space.
  // The size of the visible area is the clip view size
  nsRect visibleRect = aScrollableView->View()->GetBounds();
  aScrollableView->GetScrollPosition(visibleRect.x, visibleRect.y);

  // The actual scroll offsets
  nscoord scrollOffsetX = visibleRect.x;
  nscoord scrollOffsetY = visibleRect.y;

  nsPresContext::ScrollbarStyles ss =
    nsLayoutUtils::ScrollbarStylesOfView(aScrollableView);

  // See how aRect should be positioned vertically
  if (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN) {
    if (NS_PRESSHELL_SCROLL_ANYWHERE == aVPercent) {
      // The caller doesn't care where aRect is positioned vertically,
      // so long as it's fully visible
      if (aRect.y < visibleRect.y) {
        // Scroll up so aRect's top edge is visible
        scrollOffsetY = aRect.y;
      } else if (aRect.YMost() > visibleRect.YMost()) {
        // Scroll down so aRect's bottom edge is visible. Make sure
        // aRect's top edge is still visible
        scrollOffsetY += aRect.YMost() - visibleRect.YMost();
        if (scrollOffsetY > aRect.y) {
          scrollOffsetY = aRect.y;
        }
      }
    } else {
      // Align the aRect edge according to the specified percentage
      nscoord frameAlignY = aRect.y + (aRect.height * aVPercent) / 100;
      scrollOffsetY = frameAlignY - (visibleRect.height * aVPercent) / 100;
    }
  }

  // See how the aRect should be positioned horizontally
  if (ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
    if (NS_PRESSHELL_SCROLL_ANYWHERE == aHPercent) {
      // The caller doesn't care where the aRect is positioned horizontally,
      // so long as it's fully visible
      if (aRect.x < visibleRect.x) {
        // Scroll left so the aRect's left edge is visible
        scrollOffsetX = aRect.x;
      } else if (aRect.XMost() > visibleRect.XMost()) {
        // Scroll right so the aRect's right edge is visible. Make sure the
        // aRect's left edge is still visible
        scrollOffsetX += aRect.XMost() - visibleRect.XMost();
        if (scrollOffsetX > aRect.x) {
          scrollOffsetX = aRect.x;
        }
      }
        
    } else {
      // Align the aRect edge according to the specified percentage
      nscoord frameAlignX = aRect.x + (aRect.width * aHPercent) / 100;
      scrollOffsetX = frameAlignX - (visibleRect.width * aHPercent) / 100;
    }
  }

  aScrollableView->ScrollTo(scrollOffsetX, scrollOffsetY, 0);

  if (aScrollParentViews)
  {
    //
    // Get aScrollableView's scrolled view.
    //

    nsIView *scrolledView = 0;

    rv = aScrollableView->GetScrolledView(scrolledView);

    if (NS_FAILED(rv))
      return rv;

    if (!scrolledView)
      return NS_ERROR_FAILURE;

    //
    // Check if aScrollableRect has a parent scrollable view!
    //

    nsIView *view = aScrollableView->View()->GetParent();

    if (view)
    {
      nsIScrollableView *parentSV =
        nsLayoutUtils::GetNearestScrollingView(view, nsLayoutUtils::eEither);

      if (parentSV)
      {
        // 
        // Clip the x dimensions of aRect so that they are
        // completely within the bounds of the scrolledView.
        // This helps avoid unnecessary scrolling of parent
        // scrolled views.
        //
        nsRect svRect = scrolledView->GetBounds() - scrolledView->GetPosition();
        nsPoint topLeft = aRect.TopLeft();
        nsPoint bottomRight = aRect.BottomRight();
        ClampPointInsideRect(topLeft, svRect);
        ClampPointInsideRect(bottomRight, svRect);
        nsRect newRect(topLeft.x, topLeft.y, bottomRight.x - topLeft.x,
                       bottomRight.y - topLeft.y);

        //
        // We have a parent scrollable view, so now map aRect
        // into it's scrolled view's coordinate space.
        //

        rv = parentSV->GetScrolledView(view);

        if (NS_FAILED(rv))
          return rv;

        if (!view)
          return NS_ERROR_FAILURE;

        nscoord offsetX, offsetY;
        rv = GetViewAncestorOffset(scrolledView, view, &offsetX, &offsetY);

        if (NS_FAILED(rv))
          return rv;

        newRect.x     += offsetX;
        newRect.y     += offsetY;

        //
        // Now scroll the rect into the parent's view.
        //

        rv = ScrollRectIntoView(parentSV, newRect, aVPercent, aHPercent, aScrollParentViews);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsTypedSelection::ScrollSelectionIntoViewEvent::Run()
{
  if (!mTypedSelection)
    return NS_OK;  // event revoked

  mTypedSelection->mScrollEvent.Forget();
  mTypedSelection->ScrollIntoView(mRegion, PR_TRUE, PR_TRUE);
  return NS_OK;
}

nsresult
nsTypedSelection::PostScrollSelectionIntoViewEvent(SelectionRegion aRegion)
{
  // If we've already posted an event, revoke it and place a new one at the
  // end of the queue to make sure that any new pending reflow events are
  // processed before we scroll. This will insure that we scroll to the
  // correct place on screen.
  mScrollEvent.Revoke();

  nsRefPtr<ScrollSelectionIntoViewEvent> ev =
      new ScrollSelectionIntoViewEvent(this, aRegion);
  nsresult rv = NS_DispatchToCurrentThread(ev);
  NS_ENSURE_SUCCESS(rv, rv);

  mScrollEvent = ev;
  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::ScrollIntoView(SelectionRegion aRegion, PRBool aIsSynchronous,
                                 PRInt16 aVPercent, PRInt16 aHPercent)
{
  return ScrollIntoView(aRegion, aIsSynchronous, PR_FALSE,
                        aVPercent, aHPercent);
}

nsresult
nsTypedSelection::ScrollIntoView(SelectionRegion aRegion,
                                 PRBool aIsSynchronous, PRBool aDoFlush,
                                 PRInt16 aVPercent, PRInt16 aHPercent)
{
  nsresult result;
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  if (mFrameSelection->GetBatching())
    return NS_OK;

  if (!aIsSynchronous)
    return PostScrollSelectionIntoViewEvent(aRegion);

  //
  // Shut the caret off before scrolling to avoid
  // leaving caret turds on the screen!
  //
  nsCOMPtr<nsIPresShell> presShell;
  result = GetPresShell(getter_AddRefs(presShell));
  if (NS_FAILED(result) || !presShell)
    return result;
  nsRefPtr<nsCaret> caret;
  presShell->GetCaret(getter_AddRefs(caret));
  if (caret)
  {
    // Now that text frame character offsets are always valid (though not
    // necessarily correct), the worst that will happen if we don't flush here
    // is that some callers might scroll to the wrong place.  Those should
    // either manually flush if they're in a safe position for it or use the
    // async version of this method.
    if (aDoFlush) {
      presShell->FlushPendingNotifications(Flush_Layout);

      // Reget the presshell, since it might have gone away.
      result = GetPresShell(getter_AddRefs(presShell));
      if (NS_FAILED(result) || !presShell)
        return result;
    }

    StCaretHider  caretHider(caret);      // stack-based class hides and shows the caret

    //
    // Scroll the selection region into view.
    //

    nsRect rect;
    nsIScrollableView *scrollableView = 0;

    result = GetSelectionRegionRectAndScrollableView(aRegion, &rect, &scrollableView);

    if (NS_FAILED(result))
      return result;

    //
    // It's ok if we don't have a scrollable view, just return early.
    //
    if (!scrollableView)
      return NS_OK;

    result = ScrollRectIntoView(scrollableView, rect, aVPercent, aHPercent,
                                PR_TRUE);
  }
  return result;
}



NS_IMETHODIMP
nsTypedSelection::AddSelectionListener(nsISelectionListener* aNewListener)
{
  if (!aNewListener)
    return NS_ERROR_NULL_POINTER;
  return mSelectionListeners.AppendObject(aNewListener) ? NS_OK : NS_ERROR_FAILURE;      // addrefs
}



NS_IMETHODIMP
nsTypedSelection::RemoveSelectionListener(nsISelectionListener* aListenerToRemove)
{
  if (!aListenerToRemove )
    return NS_ERROR_NULL_POINTER;
  return mSelectionListeners.RemoveObject(aListenerToRemove) ? NS_OK : NS_ERROR_FAILURE; // releases
}


nsresult
nsTypedSelection::NotifySelectionListeners()
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do
 
  if (mFrameSelection->GetBatching()){
    mFrameSelection->SetDirty();
    return NS_OK;
  }
  PRInt32 cnt = mSelectionListeners.Count();
  nsCOMArray<nsISelectionListener> selectionListeners(mSelectionListeners);
  
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = GetPresShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell)
    domdoc = do_QueryInterface(shell->GetDocument());
  short reason = mFrameSelection->PopReason();
  for (PRInt32 i = 0; i < cnt; i++)
  {
    nsISelectionListener* thisListener = selectionListeners[i];
    if (thisListener)
      thisListener->NotifySelectionChanged(domdoc, this, reason);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTypedSelection::StartBatchChanges()
{
  if (mFrameSelection)
    mFrameSelection->StartBatchChanges();

  return NS_OK;
}



NS_IMETHODIMP
nsTypedSelection::EndBatchChanges()
{
  if (mFrameSelection)
    mFrameSelection->EndBatchChanges();

  return NS_OK;
}



NS_IMETHODIMP
nsTypedSelection::DeleteFromDocument()
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do
  return mFrameSelection->DeleteFromDocument();
}

/** SelectionLanguageChange modifies the cursor Bidi level after a change in keyboard direction
 *  @param aLangRTL is PR_TRUE if the new language is right-to-left or PR_FALSE if the new language is left-to-right
 */
NS_IMETHODIMP
nsTypedSelection::SelectionLanguageChange(PRBool aLangRTL)
{
  if (!mFrameSelection)
    return NS_ERROR_NOT_INITIALIZED; // Can't do selection
  nsresult result;
  nsIFrame *focusFrame = 0;

  result = GetPrimaryFrameForFocusNode(&focusFrame, nsnull, PR_FALSE);
  if (NS_FAILED(result)) {
    return result;
  }
  if (!focusFrame) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 frameStart, frameEnd;
  focusFrame->GetOffsets(frameStart, frameEnd);
  nsCOMPtr<nsPresContext> context;
  PRUint8 levelBefore, levelAfter;
  result = GetPresContext(getter_AddRefs(context));
  if (NS_FAILED(result) || !context)
    return result?result:NS_ERROR_FAILURE;

  PRUint8 level = NS_GET_EMBEDDING_LEVEL(focusFrame);
  PRInt32 focusOffset = GetFocusOffset();
  if ((focusOffset != frameStart) && (focusOffset != frameEnd))
    // the cursor is not at a frame boundary, so the level of both the characters (logically) before and after the cursor
    //  is equal to the frame level
    levelBefore = levelAfter = level;
  else {
    // the cursor is at a frame boundary, so use GetPrevNextBidiLevels to find the level of the characters
    //  before and after the cursor
    nsCOMPtr<nsIContent> focusContent = do_QueryInterface(GetFocusNode());
    /*
    nsFrameSelection::HINT hint;

    if ((focusOffset == frameStart && level)        // beginning of an RTL frame
        || (focusOffset == frameEnd && !level)) {   // end of an LTR frame
      hint = nsFrameSelection::HINTRIGHT;
    }
    else {                                          // end of an RTL frame or beginning of an LTR frame
      hint = nsFrameSelection::HINTLEFT;
    }
    mFrameSelection->SetHint(hint);
    */
    nsPrevNextBidiLevels levels = mFrameSelection->
      GetPrevNextBidiLevels(focusContent, focusOffset, PR_FALSE);
      
    levelBefore = levels.mLevelBefore;
    levelAfter = levels.mLevelAfter;
  }

  if ((levelBefore & 1) == (levelAfter & 1)) {
    // if cursor is between two characters with the same orientation, changing the keyboard language
    //  must toggle the cursor level between the level of the character with the lowest level
    //  (if the new language corresponds to the orientation of that character) and this level plus 1
    //  (if the new language corresponds to the opposite orientation)
    if ((level != levelBefore) && (level != levelAfter))
      level = PR_MIN(levelBefore, levelAfter);
    if ((level & 1) == aLangRTL)
      mFrameSelection->SetCaretBidiLevel(level);
    else
      mFrameSelection->SetCaretBidiLevel(level + 1);
  }
  else {
    // if cursor is between characters with opposite orientations, changing the keyboard language must change
    //  the cursor level to that of the adjacent character with the orientation corresponding to the new language.
    if ((levelBefore & 1) == aLangRTL)
      mFrameSelection->SetCaretBidiLevel(levelBefore);
    else
      mFrameSelection->SetCaretBidiLevel(levelAfter);
  }
  
  // The caret might have moved, so invalidate the desired X position
  // for future usages of up-arrow or down-arrow
  mFrameSelection->InvalidateDesiredX();
  
  return NS_OK;
}


// nsAutoCopyListener

nsAutoCopyListener* nsAutoCopyListener::sInstance = nsnull;

NS_IMPL_ISUPPORTS1(nsAutoCopyListener, nsISelectionListener)

/*
 * What we do now:
 * On every selection change, we copy to the clipboard anew, creating a
 * HTML buffer, a transferable, an nsISupportsString and
 * a huge mess every time.  This is basically what nsPresShell::DoCopy does
 * to move the selection into the clipboard for Edit->Copy.
 * 
 * What we should do, to make our end of the deal faster:
 * Create a singleton transferable with our own magic converter.  When selection
 * changes (use a quick cache to detect ``real'' changes), we put the new
 * nsISelection in the transferable.  Our magic converter will take care of
 * transferable->whatever-other-format when the time comes to actually
 * hand over the clipboard contents.
 *
 * Other issues:
 * - which X clipboard should we populate?
 * - should we use a different one than Edit->Copy, so that inadvertant
 *   selections (or simple clicks, which currently cause a selection
 *   notification, regardless of if they're in the document which currently has
 *   selection!) don't lose the contents of the ``application''?  Or should we
 *   just put some intelligence in the ``is this a real selection?'' code to
 *   protect our selection against clicks in other documents that don't create
 *   selections?
 * - maybe we should just never clear the X clipboard?  That would make this 
 *   problem just go away, which is very tempting.
 */

NS_IMETHODIMP
nsAutoCopyListener::NotifySelectionChanged(nsIDOMDocument *aDoc,
                                           nsISelection *aSel, PRInt16 aReason)
{
  if (!(aReason & nsISelectionListener::MOUSEUP_REASON   || 
        aReason & nsISelectionListener::SELECTALL_REASON ||
        aReason & nsISelectionListener::KEYPRESS_REASON))
    return NS_OK; //dont care if we are still dragging

  PRBool collapsed;
  if (!aDoc || !aSel ||
      NS_FAILED(aSel->GetIsCollapsed(&collapsed)) || collapsed) {
#ifdef DEBUG_CLIPBOARD
    fprintf(stderr, "CLIPBOARD: no selection/collapsed selection\n");
#endif
    /* clear X clipboard? */
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // call the copy code
  return nsCopySupport::HTMLCopy(aSel, doc, nsIClipboard::kSelectionClipboard);
}
