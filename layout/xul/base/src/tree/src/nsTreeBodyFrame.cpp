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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 * Contributor(s):
 *  Ben Goodger <ben@netscape.com>
 *  Joe Hewitt <hewitt@netscape.com>
 *  Jan Varga <varga@utcru.sk>
 *  Dean Tessman <dean_tessman@hotmail.com>
 *  Brian Ryner <bryner@netscape.com>
 *  Blake Ross <blaker@netscape.com>
 *  Pierre Chanial <pierrechanial@netscape.net>
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

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIPresContext.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollbarFrame.h"

#include "nsTreeBodyFrame.h"
#include "nsTreeSelection.h"

#include "nsXULAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsHTMLAtoms.h"

#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIBoxObject.h"
#include "nsGUIEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsICSSStyleRule.h"
#include "nsCSSRendering.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsIXULTemplateBuilder.h"
#include "nsXPIDLString.h"
#include "nsHTMLContainerFrame.h"
#include "nsIView.h"
#include "nsWidgetsCID.h"
#include "nsBoxFrame.h"
#include "nsBoxObject.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsBoxLayoutState.h"
#include "nsIDragService.h"
#include "nsTreeContentView.h"
#include "nsTreeUtils.h"
#include "nsChildIterator.h"
#include "nsIScrollableView.h"
#include "nsITheme.h"
#include "nsITimelineService.h"
#include "nsITimerInternal.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgIContainerObserver.h"
#include "imgILoader.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif

#define ELLIPSIS "..."

// Delay (ms) for opening spring-loaded folders.
static const PRUint32 kOpenDelay = 1000;
// Delay (ms) for triggering the tree scrolling.
static const PRUint32 kLazyScrollDelay = 150;
// Delay (ms) for scrolling the tree.
static const PRUint32 kScrollDelay = 100;

// The style context cache impl
nsresult 
nsTreeStyleCache::GetStyleContext(nsICSSPseudoComparator* aComparator,
                                      nsIPresContext* aPresContext, nsIContent* aContent, 
                                      nsIStyleContext* aContext, nsIAtom* aPseudoElement,
                                      nsISupportsArray* aInputWord,
                                      nsIStyleContext** aResult)
{
  *aResult = nsnull;
  
  PRUint32 count;
  aInputWord->Count(&count);
  nsDFAState startState(0);
  nsDFAState* currState = &startState;

  // Go ahead and init the transition table.
  if (!mTransitionTable) {
    // Automatic miss. Build the table
    mTransitionTable =
      new nsObjectHashtable(nsnull, nsnull, DeleteDFAState, nsnull);
  }

  // The first transition is always made off the supplied pseudo-element.
  nsTransitionKey key(currState->GetStateID(), aPseudoElement);
  currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

  if (!currState) {
    // We had a miss. Make a new state and add it to our hash.
    currState = new nsDFAState(mNextState);
    mNextState++;
    mTransitionTable->Put(&key, currState);
  }

  for (PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIAtom> pseudo = getter_AddRefs(NS_STATIC_CAST(nsIAtom*, aInputWord->ElementAt(i)));
    nsTransitionKey key(currState->GetStateID(), pseudo);
    currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

    if (!currState) {
      // We had a miss. Make a new state and add it to our hash.
      currState = new nsDFAState(mNextState);
      mNextState++;
      mTransitionTable->Put(&key, currState);
    }
  }

  // We're in a final state.
  // Look up our style context for this state.
   if (mCache)
    *aResult = NS_STATIC_CAST(nsIStyleContext*, mCache->Get(currState)); // Addref occurs on *aResult.
  if (!*aResult) {
    // We missed the cache. Resolve this pseudo-style.
    aPresContext->ResolvePseudoStyleWithComparator(aContent, aPseudoElement,
                                                   aContext, aComparator,
                                                   aResult); // Addref occurs on *aResult.
    // Put it in our table.
    if (!mCache)
      mCache = new nsSupportsHashtable;
    mCache->Put(currState, *aResult);
  }

  return NS_OK;
}

/* static */ PRBool PR_CALLBACK
nsTreeStyleCache::DeleteDFAState(nsHashKey *aKey,
                                     void *aData,
                                     void *closure)
{
  nsDFAState* entry = NS_STATIC_CAST(nsDFAState*, aData);
  delete entry;
  return PR_TRUE;
}

// Column class that caches all the info about our column.
nsTreeColumn::nsTreeColumn(nsIContent* aColElement, nsIFrame* aFrame)
:mNext(nsnull)
{
  mColFrame = aFrame;
  mColElement = aColElement; 

  // Fetch the ID.
  mColElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, mID);

  // If we have an ID, cache the ID as an atom.
  if (!mID.IsEmpty()) {
    mIDAtom = getter_AddRefs(NS_NewAtom(mID));
  }

  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext));

  const nsStyleVisibility* vis = 
    (const nsStyleVisibility*)styleContext->GetStyleData(eStyleStruct_Visibility);

  // Fetch the crop style.
  mCropStyle = 0;
  nsAutoString crop;
  mColElement->GetAttr(kNameSpaceID_None, nsXULAtoms::crop, crop);
  if (crop.EqualsIgnoreCase("center"))
    mCropStyle = 1;
  else if (crop.EqualsIgnoreCase("left") || crop.EqualsIgnoreCase("start"))
    mCropStyle = 2;

  // Cache our text alignment policy.
  const nsStyleText* textStyle =
        (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  mTextAlignment = textStyle->mTextAlign;
  if (mTextAlignment == 0 || mTextAlignment == 2) { // Left or Right
    if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
      mTextAlignment = 2 - mTextAlignment; // Right becomes left, left becomes right.
  }

  // Figure out if we're the primary column (that has to have indentation
  // and twisties drawn.
  mIsPrimaryCol = PR_FALSE;
  nsAutoString primary;
  mColElement->GetAttr(kNameSpaceID_None, nsXULAtoms::primary, primary);
  if (primary.EqualsIgnoreCase("true"))
    mIsPrimaryCol = PR_TRUE;

  // Figure out if we're a cycling column (one that doesn't cause a selection
  // to happen).
  mIsCyclerCol = PR_FALSE;
  nsAutoString cycler;
  mColElement->GetAttr(kNameSpaceID_None, nsXULAtoms::cycler, cycler);
  if (cycler.EqualsIgnoreCase("true"))
    mIsCyclerCol = PR_TRUE;

  // Figure out our column type. Default type is text.
  mType = eText;
  nsAutoString type;
  mColElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
  if (type.EqualsIgnoreCase("checkbox"))
    mType = eCheckbox;
  else if (type.EqualsIgnoreCase("progressmeter"))
    mType = eProgressMeter;

  // Cache our index.
  mColIndex = -1;
  nsCOMPtr<nsIContent> parent;
  mColElement->GetParent(*getter_AddRefs(parent));
  PRInt32 count;
  parent->ChildCount(count);
  PRInt32 j = 0;
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child;
    parent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (tag == nsXULAtoms::treecol) {
      if (child == mColElement) {
        mColIndex = j;
        break;
      }
      j++;
    }
  }
}

inline nscoord nsTreeColumn::GetWidth()
{
  if (mColFrame) {
    nsRect rect;
    mColFrame->GetRect(rect);
    return rect.width;
  }
  return 0;
}

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeBodyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeBodyFrame* it = new (aPresShell) nsTreeBodyFrame(aPresShell);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeFrame


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsTreeBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsITreeBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsICSSPseudoComparator)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
  NS_INTERFACE_MAP_ENTRY(nsIReflowCallback)
NS_INTERFACE_MAP_END_INHERITING(nsLeafBoxFrame)



// Constructor
nsTreeBodyFrame::nsTreeBodyFrame(nsIPresShell* aPresShell)
:nsLeafBoxFrame(aPresShell), mPresContext(nsnull), mTreeBoxObject(nsnull), mImageCache(nsnull),
 mColumns(nsnull), mScrollbar(nsnull), mTopRowIndex(0), mRowHeight(0), mIndentation(0), mStringWidth(-1),
 mFocused(PR_FALSE), mColumnsDirty(PR_TRUE), mDropAllowed(PR_FALSE), mHasFixedRowCount(PR_FALSE),
 mVerticalOverflow(PR_FALSE), mImageGuard(PR_FALSE), mReflowCallbackPosted(PR_FALSE),
 mDropRow(-1), mDropOrient(-1), mScrollLines(0), mTimer(nsnull)
{
  NS_NewISupportsArray(getter_AddRefs(mScratchArray));
}

// Destructor
nsTreeBodyFrame::~nsTreeBodyFrame()
{
  delete mImageCache;
}

NS_IMETHODIMP_(nsrefcnt) 
nsTreeBodyFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsTreeBodyFrame::Release(void)
{
  return NS_OK;
}

static nsIFrame* InitScrollbarFrame(nsIPresContext* aPresContext, nsIFrame* aCurrFrame, nsIScrollbarMediator* aSM)
{
  // Check ourselves
  nsCOMPtr<nsIScrollbarFrame> sf(do_QueryInterface(aCurrFrame));
  if (sf) {
    sf->SetScrollbarMediator(aSM);
    return aCurrFrame;
  }

  nsIFrame* child;
  aCurrFrame->FirstChild(aPresContext, nsnull, &child);
  while (child) {
    nsIFrame* result = InitScrollbarFrame(aPresContext, child, aSM);
    if (result)
      return result;
    child->GetNextSibling(&child);
  }

  return nsnull;
}

NS_IMETHODIMP
nsTreeBodyFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                          nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  mPresContext = aPresContext;
  nsresult rv = nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  nsBoxFrame::CreateViewForFrame(aPresContext, this, aContext, PR_TRUE);
  nsIView* ourView;
  nsLeafBoxFrame::GetView(aPresContext, &ourView);

  static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

  ourView->CreateWidget(kWidgetCID);
  ourView->GetWidget(*getter_AddRefs(mTreeWidget));
  mIndentation = GetIndentation();
  mRowHeight = GetRowHeight();
  return rv;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  EnsureView();

  nsCOMPtr<nsIContent> baseElement;
  GetBaseElement(getter_AddRefs(baseElement));
  nsCOMPtr<nsIAtom> tag;
  baseElement->GetTag(*getter_AddRefs(tag));

  PRInt32 desiredRows;
  if (tag == nsHTMLAtoms::select) {
    aSize.width = CalcMaxRowWidth(aBoxLayoutState);
    nsAutoString size;
    baseElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, size);
    if (!size.IsEmpty()) {
      PRInt32 err;
      desiredRows = size.ToInteger(&err);
      mHasFixedRowCount = PR_TRUE;
      mPageCount = desiredRows;
    } else
      desiredRows = 1;
  } else {
    // tree
    aSize.width = 0;
    nsAutoString rows;
    baseElement->GetAttr(kNameSpaceID_None, nsXULAtoms::rows, rows);
    if (!rows.IsEmpty()) {
      PRInt32 err;
      desiredRows = rows.ToInteger(&err);
      mHasFixedRowCount = PR_TRUE;
      mPageCount = desiredRows;
    } else
      desiredRows = 0;
  }

  aSize.height = mRowHeight * desiredRows;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize);

  return NS_OK;
}

nscoord
nsTreeBodyFrame::CalcMaxRowWidth(nsBoxLayoutState& aState)
{
  if (mStringWidth != -1)
    return mStringWidth;

  if (!mView)
    return 0;

  nsCOMPtr<nsIStyleContext> rowContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow, getter_AddRefs(rowContext));
  nsMargin rowMargin(0,0,0,0);
  nsStyleBorderPadding  bPad;
  rowContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(rowMargin);

  PRInt32 numRows;
  mView->GetRowCount(&numRows);

  nscoord rowWidth;
  nsTreeColumn* col;
  EnsureColumns();

  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIRenderingContext> rc;
  presShell->CreateRenderingContext(this, getter_AddRefs(rc));

  for (PRInt32 row = 0; row < numRows; ++row) {
    rowWidth = 0;
    col = mColumns;

    while (col) {
      nscoord desiredWidth, currentWidth;
      GetCellWidth(row, col->GetID(), rc, desiredWidth, currentWidth);
      rowWidth += desiredWidth;
      col = col->GetNext();
    }

    if (rowWidth > mStringWidth)
      mStringWidth = rowWidth;
  }

  mStringWidth += rowMargin.left + rowMargin.right;
  return mStringWidth;
}

NS_IMETHODIMP
nsTreeBodyFrame::Destroy(nsIPresContext* aPresContext)
{
  // Make sure we cancel any posted callbacks. 
  if (mReflowCallbackPosted) {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    shell->CancelReflowCallback(this);
    mReflowCallbackPosted = PR_FALSE;
  }

  // Delete our column structures.
  delete mColumns;
  mColumns = nsnull;
  
  // Save off our info into the box object.
  EnsureBoxObject();
  if (mTreeBoxObject) {
    nsCOMPtr<nsIBoxObject> box(do_QueryInterface(mTreeBoxObject));
    if (mTopRowIndex > 0) {
      nsAutoString topRowStr; topRowStr.Assign(NS_LITERAL_STRING("topRow"));
      nsAutoString topRow;
      topRow.AppendInt(mTopRowIndex);
      box->SetProperty(topRowStr.get(), topRow.get());
    }

    // Always null out the cached tree body frame.
    nsAutoString treeBody(NS_LITERAL_STRING("treebody"));
    box->RemoveProperty(treeBody.get());

    mTreeBoxObject = nsnull; // Drop our ref here.
  }

  mView = nsnull;

  return nsLeafBoxFrame::Destroy(aPresContext);
}

void
nsTreeBodyFrame::EnsureBoxObject()
{
  if (!mTreeBoxObject) {
    nsCOMPtr<nsIContent> parent;
    GetBaseElement(getter_AddRefs(parent));

    if (parent) {
      nsCOMPtr<nsIDocument> parentDoc;
      parent->GetDocument(*getter_AddRefs(parentDoc));
      if (!parentDoc) // there may be no document, if we're called from Destroy()
        return;
      
      nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(parentDoc);
      nsCOMPtr<nsIBoxObject> box;
      nsCOMPtr<nsIDOMElement> domElem = do_QueryInterface(parent);
      nsDoc->GetBoxObjectFor(domElem, getter_AddRefs(box));
      
      if (box) {
        nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
        SetBoxObject(treeBox);
      }
    }
  }
}

void
nsTreeBodyFrame::EnsureView()
{
  if (!mView) {
    EnsureBoxObject();
    nsCOMPtr<nsIBoxObject> box = do_QueryInterface(mTreeBoxObject);
    if (box) {
      nsCOMPtr<nsISupports> suppView;
      box->GetPropertyAsSupports(NS_LITERAL_STRING("view").get(), getter_AddRefs(suppView));
      nsCOMPtr<nsITreeView> treeView(do_QueryInterface(suppView));

      if (treeView) {
        nsXPIDLString rowStr;
        box->GetProperty(NS_LITERAL_STRING("topRow").get(), getter_Copies(rowStr));
        nsAutoString rowStr2(rowStr);
        PRInt32 error;
        PRInt32 rowIndex = rowStr2.ToInteger(&error);

        // Set our view.
        SetView(treeView);

        // Scroll to the given row.
        // XXX is this optimal if we haven't laid out yet?
        ScrollToRow(rowIndex);

        // Clear out the property info for the top row, but we always keep the
        // view current.
        box->RemoveProperty(NS_LITERAL_STRING("topRow").get());
      }
    }

    if (!mView) {
      // If we don't have a box object yet, or no view was set on it,
      // look for a XULTreeBuilder or create a content view.
      
      nsCOMPtr<nsIContent> parent;
      mContent->GetParent(*getter_AddRefs(parent));
      nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(parent);
      if (xulele) {
        nsCOMPtr<nsITreeView> view;

        // See if there is a XUL tree builder associated with
        // the parent element.
        nsCOMPtr<nsIXULTemplateBuilder> builder;
        xulele->GetBuilder(getter_AddRefs(builder));
        if (builder)
          view = do_QueryInterface(builder);

        if (!view) {
          // No tree builder, create an tree content view.
          nsCOMPtr<nsITreeContentView> contentView;
          NS_NewTreeContentView(getter_AddRefs(contentView));
          if (contentView)
            view = do_QueryInterface(contentView);
        }

        // Hook up the view.
        if (view)
          SetView(view);
      }
    }
  }
}

NS_IMETHODIMP
nsTreeBodyFrame::SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect)
{
  if (aRect != mRect && !mReflowCallbackPosted) {
    mReflowCallbackPosted = PR_TRUE;
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    shell->PostReflowCallback(this);
  }

  return nsLeafBoxFrame::SetBounds(aBoxLayoutState, aRect);
}


NS_IMETHODIMP
nsTreeBodyFrame::ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag)
{
  if (mView) {
    mInnerBox = GetInnerBox();
    if (!mHasFixedRowCount)
      mPageCount = mInnerBox.height / mRowHeight;

    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    PRInt32 lastPageTopRow = PR_MAX(0, rowCount - mPageCount);
    if (mTopRowIndex > lastPageTopRow)
      ScrollToRow(lastPageTopRow);

    // make sure that the current selected item is still
    // visible after the tree changes size.
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) {
      PRInt32 currentIndex;
      sel->GetCurrentIndex(&currentIndex);
      if (currentIndex != -1)
        EnsureRowIsVisible(currentIndex);
    }

    InvalidateScrollbar();
    CheckVerticalOverflow();
  }

  mReflowCallbackPosted = PR_FALSE;
  *aFlushFlag = PR_FALSE;

  return NS_OK;
}


static void 
AdjustForBorderPadding(nsIStyleContext* aContext, nsRect& aRect)
{
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  aContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(m);
  aRect.Deflate(m);
}

NS_IMETHODIMP nsTreeBodyFrame::GetView(nsITreeView * *aView)
{
  EnsureView();
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::SetView(nsITreeView * aView)
{
  // First clear out the old view.
  EnsureBoxObject();
  nsCOMPtr<nsIBoxObject> box = do_QueryInterface(mTreeBoxObject);
  
  nsAutoString view(NS_LITERAL_STRING("view"));
  
  if (mView) {
    mView->SetTree(nsnull);
    mView = nsnull;
    box->RemoveProperty(view.get());

    // Only reset the top row index and delete the columns if we had an old non-null view.
    mTopRowIndex = 0;
    delete mColumns;
    mColumns = nsnull;
  }

  // Tree, meet the view.
  mView = aView;
 
  // Changing the view causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the tree.
  Invalidate();
 
  if (mView) {
    // View, meet the tree.
    mView->SetTree(mTreeBoxObject);
 
    box->SetPropertyAsSupports(view.get(), mView);

    // Give the view a new empty selection object to play with, but only if it
    // doesn't have one already.
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (!sel) {
      NS_NewTreeSelection(mTreeBoxObject, getter_AddRefs(sel));
      mView->SetSelection(sel);
    }

    // The scrollbar will need to be updated.
    InvalidateScrollbar();

    // Reset scrollbar position.
    UpdateScrollbar();

    CheckVerticalOverflow();
  }
 
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetFocused(PRBool* aFocused)
{
  *aFocused = mFocused;
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::SetFocused(PRBool aFocused)
{
  if (mFocused != aFocused) {
    mFocused = aFocused;
    if (mView) {
      nsCOMPtr<nsITreeSelection> sel;
      mView->GetSelection(getter_AddRefs(sel));
      if (sel)
        sel->InvalidateSelection();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetTreeBody(nsIDOMElement** aElement)
{
  //NS_ASSERTION(mContent, "no content, see bug #104878");
  if (!mContent)
    return NS_ERROR_NULL_POINTER;

  return mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aElement);
}

NS_IMETHODIMP nsTreeBodyFrame::GetSelection(nsITreeSelection** aSelection)
{
  if (mView)
    return mView->GetSelection(aSelection);

  *aSelection = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetRowHeight(PRInt32* _retval)
{
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *_retval = NSToCoordRound((float) mRowHeight * t2p);

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetColumnIndex(const PRUnichar *aColID, PRInt32 *_retval)
{
  *_retval = -1;
  for (nsTreeColumn* currCol = mColumns; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetID().Equals(aColID)) {
      *_retval = currCol->GetColIndex();
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetColumnID(PRInt32 colIndex, nsAString & _retval)
{
  _retval = NS_LITERAL_STRING("");
  for (nsTreeColumn* currCol = mColumns; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetColIndex() == colIndex) {
      _retval = currCol->GetID();
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetKeyColumnIndex(PRInt32 *_retval)
{
  nsAutoString attr;
  PRInt32 first, primary, sorted;

  EnsureColumns();
  first = primary = sorted = -1;
  for (nsTreeColumn* currCol = mColumns; currCol; currCol = currCol->GetNext()) {
    // Skip hidden column
    currCol->GetElement()->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, attr);
    if (attr.EqualsIgnoreCase("true"))
      continue;

    // Skip non-text column
    if (currCol->GetType() != nsTreeColumn::eText)
      continue;

    if (first == -1)
      first = currCol->GetColIndex();
    
    currCol->GetElement()->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, attr);
    if (attr.Length() > 0) { // Use sorted column as the primary
      sorted = currCol->GetColIndex();
      break;
    }

    if (currCol->IsPrimary())
      if (primary == -1)
        primary = currCol->GetColIndex();
  }

  if (sorted >= 0)
    *_retval = sorted;
  else if (primary >= 0)
    *_retval = primary;
  else
    *_retval = first;

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetFirstVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex;
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetLastVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex + mPageCount;
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetPageCount(PRInt32 *_retval)
{
  *_retval = mPageCount;
  return NS_OK;
}


NS_IMETHODIMP nsTreeBodyFrame::Invalidate()
{
  if (!mRect.IsEmpty()) {
    nsLeafBoxFrame::Invalidate(mPresContext, mRect, PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::InvalidateColumn(const PRUnichar *aColID)
{
  nscoord currX = mInnerBox.x;
  for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {
    if (currCol->GetID().Equals(aColID)) {
      nsRect columnRect(currX, mInnerBox.y, currCol->GetWidth(), mInnerBox.height);
      nsLeafBoxFrame::Invalidate(mPresContext, columnRect, PR_FALSE);
      break;
    }
    currX += currCol->GetWidth();
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::InvalidateRow(PRInt32 aIndex)
{
  if (aIndex < mTopRowIndex || aIndex > mTopRowIndex + mPageCount + 1)
    return NS_OK;

  nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*(aIndex-mTopRowIndex), mInnerBox.width, mRowHeight);
  if (!rowRect.IsEmpty())
    nsLeafBoxFrame::Invalidate(mPresContext, rowRect, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::InvalidateCell(PRInt32 aIndex, const PRUnichar *aColID)
{
  if (aIndex < mTopRowIndex || aIndex > mTopRowIndex + mPageCount + 1)
    return NS_OK;

  if (mImageGuard)
    return NS_OK;

  nscoord currX = mInnerBox.x;
  nscoord yPos = mInnerBox.y+mRowHeight*(aIndex-mTopRowIndex);
  for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {

    if (currCol->GetID().Equals(aColID)) {
      nsRect cellRect(currX, yPos, currCol->GetWidth(), mRowHeight);
      nsLeafBoxFrame::Invalidate(mPresContext, cellRect, PR_FALSE);
      break;
    }
    currX += currCol->GetWidth();
  }
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::InvalidatePrimaryCell(PRInt32 aIndex)
{
  if (aIndex < mTopRowIndex || aIndex > mTopRowIndex + mPageCount + 1)
    return NS_OK;

  nscoord currX = mInnerBox.x;
  nscoord yPos = mInnerBox.y+mRowHeight*(aIndex-mTopRowIndex);
  for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {

    if (currCol->IsPrimary()) {
      nsRect cellRect(currX, yPos, currCol->GetWidth(), mRowHeight);
#if defined(XP_MAC) || defined(XP_MACOSX)
      // Mac can't process the event loop during a drag, so if we're dragging,
      // invalidate synchronously.
      nsLeafBoxFrame::Invalidate(mPresContext, cellRect, mDragSession ? PR_TRUE : PR_FALSE);
#else
      nsLeafBoxFrame::Invalidate(mPresContext, cellRect, PR_FALSE);
#endif
      break;
    }
    currX += currCol->GetWidth();
  }
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  if (aStart == aEnd)
    return InvalidateRow(aStart);

  PRInt32 last;
  GetLastVisibleRow(&last);
  if (aEnd < mTopRowIndex || aStart > last)
    return NS_OK;

  if (aStart < mTopRowIndex)
    aStart = mTopRowIndex;

  if (aEnd > last)
    aEnd = last;

  nsRect rangeRect(mInnerBox.x, mInnerBox.y+mRowHeight*(aStart-mTopRowIndex), mInnerBox.width, mRowHeight*(aEnd-aStart+1));
  nsLeafBoxFrame::Invalidate(mPresContext, rangeRect, PR_FALSE);

  return NS_OK;
}

nsIFrame*
nsTreeBodyFrame::EnsureScrollbar()
{
  if (!mScrollbar) {
    // Try to find it.
    nsCOMPtr<nsIContent> parContent;
    GetBaseElement(getter_AddRefs(parContent));
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    nsIFrame* treeFrame;
    shell->GetPrimaryFrameFor(parContent, &treeFrame);
    if (treeFrame)
      mScrollbar = InitScrollbarFrame(mPresContext, treeFrame, this);
  }

  NS_ASSERTION(mScrollbar, "no scroll bar");
  return mScrollbar;
}

void
nsTreeBodyFrame::UpdateScrollbar()
{
  // Update the scrollbar.
  if (!EnsureScrollbar())
    return;
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  nsAutoString curPos;
  curPos.AppendInt(mTopRowIndex*rowHeightAsPixels);
  scrollbarContent->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, curPos, PR_TRUE);
}

nsresult nsTreeBodyFrame::CheckVerticalOverflow()
{
  PRBool verticalOverflowChanged = PR_FALSE;

  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);
  if (!mVerticalOverflow && rowCount > mPageCount) {
    mVerticalOverflow = PR_TRUE;
    verticalOverflowChanged = PR_TRUE;
  }
  else if (mVerticalOverflow && rowCount <= mPageCount) {
    mVerticalOverflow = PR_FALSE;
    verticalOverflowChanged = PR_TRUE;
  }
 
  if (verticalOverflowChanged) {
    nsScrollPortEvent event;
    event.eventStructType = NS_SCROLLPORT_EVENT;  
    event.widget = nsnull;
    event.orient = nsScrollPortEvent::vertical;
    event.nativeMsg = nsnull;
    event.message = mVerticalOverflow ? NS_SCROLLPORT_OVERFLOW : NS_SCROLLPORT_UNDERFLOW;

    nsEventStatus status = nsEventStatus_eIgnore;
    mContent->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::InvalidateScrollbar()
{
  if (!EnsureScrollbar() || !mView)
    return NS_OK;

  PRInt32 rowCount = 0;
  mView->GetRowCount(&rowCount);
  
  nsCOMPtr<nsIContent> scrollbar;
  mScrollbar->GetContent(getter_AddRefs(scrollbar));

  nsAutoString maxposStr;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  PRInt32 size = rowHeightAsPixels*(rowCount-mPageCount);
  maxposStr.AppendInt(size);
  scrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::maxpos, maxposStr, PR_TRUE);

  // Also set our page increment and decrement.
  nscoord pageincrement = mPageCount*rowHeightAsPixels;
  nsAutoString pageStr;
  pageStr.AppendInt(pageincrement);
  scrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::pageincrement, pageStr, PR_TRUE);

  return NS_OK;
}


// Takes client x/y in pixels, converts them to twips, and massages them to be
// in our coordinate system.
void
nsTreeBodyFrame::AdjustEventCoordsToBoxCoordSpace (PRInt32 aX, PRInt32 aY, PRInt32* aResultX, PRInt32* aResultY)
{
  // Convert our x and y coords to twips.
  float pixelsToTwips = 0.0;
  mPresContext->GetPixelsToTwips(&pixelsToTwips);
  aX = NSToIntRound(aX * pixelsToTwips);
  aY = NSToIntRound(aY * pixelsToTwips);
  
  // Get our box object.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(doc));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mContent));
  
  nsCOMPtr<nsIBoxObject> boxObject;
  nsDoc->GetBoxObjectFor(elt, getter_AddRefs(boxObject));
  
  PRInt32 x;
  PRInt32 y;
  boxObject->GetX(&x);
  boxObject->GetY(&y);

  x = NSToIntRound(x * pixelsToTwips);
  y = NSToIntRound(y * pixelsToTwips);

  // Take into account the parent's scroll offset, since clientX and clientY
  // are relative to the viewport.

  nsIView* parentView;
  nsLeafBoxFrame::GetView(mPresContext, &parentView);
  parentView->GetParent(parentView);
  parentView->GetParent(parentView);

  if (parentView) {
    nsIScrollableView* scrollView = nsnull;
    CallQueryInterface(parentView, &scrollView);
    if (scrollView) {
      nscoord scrollX = 0, scrollY = 0;
      scrollView->GetScrollPosition(scrollX, scrollY);
      x -= scrollX;
      y -= scrollY;
    }
  }

  // Adjust into our coordinate space.
  x = aX-x;
  y = aY-y;

  // Adjust y by the inner box y, so that we're in the inner box's
  // coordinate space.
  y += mInnerBox.y;
  
  *aResultX = x;
  *aResultY = y;
} // AdjustEventCoordsToBoxCoordSpace

NS_IMETHODIMP nsTreeBodyFrame::GetRowAt(PRInt32 aX, PRInt32 aY, PRInt32* _retval)
{
  if (!mView)
    return NS_OK;

  PRInt32 x;
  PRInt32 y;
  AdjustEventCoordsToBoxCoordSpace (aX, aY, &x, &y);
  
  // Now just mod by our total inner box height and add to our top row index.
  *_retval = (y/mRowHeight)+mTopRowIndex;

  // Check if the coordinates are actually in our visible space.
  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);
  if (*_retval < mTopRowIndex || *_retval > PR_MIN(mTopRowIndex+mPageCount, rowCount - 1))
    *_retval = -1;

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::GetCellAt(PRInt32 aX, PRInt32 aY, PRInt32* aRow, PRUnichar** aColID,
                                             PRUnichar** aChildElt)
{
  if (!mView)
    return NS_OK;

  PRInt32 x;
  PRInt32 y;
  AdjustEventCoordsToBoxCoordSpace (aX, aY, &x, &y);
  
  // Now just mod by our total inner box height and add to our top row index.
  *aRow = (y/mRowHeight)+mTopRowIndex;

  // Check if the coordinates are actually in our visible space.
  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);
  if (*aRow < mTopRowIndex || *aRow > PR_MIN(mTopRowIndex+mPageCount, rowCount - 1)) {
    *aRow = -1;
    return NS_OK;
  }

  // Determine the column hit.
  nscoord currX = mInnerBox.x;
  for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {
    nsRect cellRect(currX, mInnerBox.y+mRowHeight*(*aRow-mTopRowIndex), currCol->GetWidth(), mRowHeight);
    PRInt32 overflow = cellRect.x+cellRect.width-(mInnerBox.x+mInnerBox.width);
    if (overflow > 0)
      cellRect.width -= overflow;

    if (x >= cellRect.x && x < cellRect.x + cellRect.width) {
      // We know the column hit now.
      *aColID = ToNewUnicode(currCol->GetID());

      if (currCol->IsCycler())
        // Cyclers contain only images.  Fill this in immediately and return.
        *aChildElt = ToNewUnicode(NS_LITERAL_STRING("image"));
      else
        GetItemWithinCellAt(x, cellRect, *aRow, currCol, aChildElt);
      break;
    }

    currX += cellRect.width;
  }

  return NS_OK;
}


//
// GetCoordsForCellItem
//
// Find the x/y location and width/height (all in PIXELS) of the given object
// in the given column. 
//
// XXX IMPORTANT XXX:
// Hyatt says in the bug for this, that the following needs to be done:
// (1) You need to deal with overflow when computing cell rects.  See other column 
// iteration examples... if you don't deal with this, you'll mistakenly extend the 
// cell into the scrollbar's rect.
//
// (2) You are adjusting the cell rect by the *row" border padding.  That's 
// wrong.  You need to first adjust a row rect by its border/padding, and then the 
// cell rect fits inside the adjusted row rect.  It also can have border/padding 
// as well as margins.  The vertical direction isn't that important, but you need 
// to get the horizontal direction right.
//
// (3) GetImageSize() does not include margins (but it does include border/padding).  
// You need to make sure to add in the image's margins as well.
//
NS_IMETHODIMP
nsTreeBodyFrame::GetCoordsForCellItem(PRInt32 aRow, const PRUnichar *aColID, const PRUnichar *aCellItem, 
                                          PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;

  nscoord currX = mInnerBox.x;

  // The Rect for the requested item. 
  nsRect theRect;

  for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x + mInnerBox.width;
       currCol = currCol->GetNext()) {

    // The Rect for the current cell. 
    nsRect cellRect(currX, mInnerBox.y + mRowHeight * (aRow - mTopRowIndex), currCol->GetWidth(), mRowHeight);

    // Check the ID of the current column to see if it matches. If it doesn't 
    // increment the current X value and continue to the next column.
    if (!currCol->GetID().Equals(aColID)) {
      currX += cellRect.width;
      continue;
    }

    // Now obtain the properties for our cell.
    PrefillPropertyArray(aRow, currCol);
    mView->GetCellProperties(aRow, currCol->GetID().get(), mScratchArray);

    nsCOMPtr<nsIStyleContext> rowContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow, getter_AddRefs(rowContext));

    // We don't want to consider any of the decorations that may be present
    // on the current row, so we have to deflate the rect by the border and 
    // padding and offset its left and top coordinates appropriately. 
    AdjustForBorderPadding(rowContext, cellRect);

    nsCOMPtr<nsIStyleContext> cellContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell, getter_AddRefs(cellContext));

    NS_NAMED_LITERAL_STRING(cell, "cell");
    if (currCol->IsCycler() || cell.Equals(aCellItem)) {
      // If the current Column is a Cycler, then the Rect is just the cell - the margins. 
      // Similarly, if we're just being asked for the cell rect, provide it. 

      theRect = cellRect;
      const nsStyleMargin* cellMarginData = (const nsStyleMargin*) cellContext->GetStyleData(eStyleStruct_Margin);
      nsMargin cellMargin;
      cellMarginData->GetMargin(cellMargin);
      theRect.Deflate(cellMargin);
      break;
    }

    // Since we're not looking for the cell, and since the cell isn't a cycler,
    // we're looking for some subcomponent, and now we need to subtract the 
    // borders and padding of the cell from cellRect so this does not 
    // interfere with our computations.
    AdjustForBorderPadding(cellContext, cellRect);

    // Now we'll start making our way across the cell, starting at the edge of 
    // the cell and proceeding until we hit the right edge. |cellX| is the 
    // working X value that we will increment as we crawl from left to right.
    nscoord cellX = cellRect.x;
    nscoord remainWidth = cellRect.width;

    if (currCol->IsPrimary()) {
      // If the current Column is a Primary, then we need to take into account the indentation
      // and possibly a twisty. 

      // The amount of indentation is the indentation width (|mIndentation|) by the level. 
      PRInt32 level;
      mView->GetLevel(aRow, &level);
      cellX += mIndentation * level;
      remainWidth -= mIndentation * level;

      PRBool hasTwisty = PR_FALSE;
      PRBool isContainer = PR_FALSE;
      mView->IsContainer(aRow, &isContainer);
      if (isContainer) {
        PRBool isContainerEmpty = PR_FALSE;
        mView->IsContainerEmpty(aRow, &isContainerEmpty);
        if (!isContainerEmpty)
          hasTwisty = PR_TRUE;
      }

      // Find the twisty rect by computing its size. 
      nsCOMPtr<nsIStyleContext> twistyContext;
      GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty, getter_AddRefs(twistyContext));

      // |GetImageSize| returns the rect of the twisty image, including the 
      // borders and padding. 
      nsRect twistyImageRect = GetImageSize(aRow, currCol->GetID().get(), PR_TRUE, twistyContext);
      if (NS_LITERAL_STRING("twisty").Equals(aCellItem)) {
        // If we're looking for the twisty Rect, just return the result of |GetImageSize|
        theRect = twistyImageRect;
        break;
      }
      
      // Now we need to add in the margins of the twisty element, so that we 
      // can find the offset of the next element in the cell. 
      const nsStyleMargin* twistyMarginData = (const nsStyleMargin*) twistyContext->GetStyleData(eStyleStruct_Margin);
      nsMargin twistyMargin;
      twistyMarginData->GetMargin(twistyMargin);
      twistyImageRect.Inflate(twistyMargin);

      // Adjust our working X value with the twisty width (image size, margins,
      // borders, padding. 
      cellX += twistyImageRect.width;
    }

    // Cell Image
    nsCOMPtr<nsIStyleContext> imageContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage, getter_AddRefs(imageContext));

    nsRect imageSize = GetImageSize(aRow, currCol->GetID().get(), PR_FALSE, imageContext);
    if (NS_LITERAL_STRING("image").Equals(aCellItem)) {
      theRect = imageSize;
      theRect.x = cellX;
      theRect.y = cellRect.y;
      break;
    }

    // Increment cellX by the image width
    cellX += imageSize.width;
    
    // Cell Text 
    nsAutoString cellText;
    mView->GetCellText(aRow, currCol->GetID().get(), cellText);

    // Create a scratch rect to represent the text rectangle, with the current 
    // X and Y coords, and a guess at the width and height. The width is the 
    // remaining width we have left to traverse in the cell, which will be the
    // widest possible value for the text rect, and the row height. 
    nsRect textRect(cellX, cellRect.y, remainWidth, mRowHeight);

    // Measure the width of the text. If the width of the text is greater than 
    // the remaining width available, then we just assume that the text has 
    // been cropped and use the remaining rect as the text Rect. Otherwise,
    // we add in borders and padding to the text dimension and give that back. 
    nsCOMPtr<nsIStyleContext> textContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext, getter_AddRefs(textContext));

    const nsStyleFont* fontStyle = (const nsStyleFont*) textContext->GetStyleData(eStyleStruct_Font);
    nsCOMPtr<nsIDeviceContext> dc;

    mPresContext->GetDeviceContext(getter_AddRefs(dc));
    nsCOMPtr<nsIFontMetrics> fm;
    dc->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fm));
    nscoord height;
    fm->GetHeight(height);

    nsStyleBorderPadding borderPadding;
    textContext->GetBorderPaddingFor(borderPadding);
    nsMargin bp(0,0,0,0);
    borderPadding.GetBorderPadding(bp);
    
    textRect.height = height + bp.top + bp.bottom;

    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    nsCOMPtr<nsIRenderingContext> rc;
    shell->CreateRenderingContext(this, getter_AddRefs(rc));
    rc->SetFont(fm);
    nscoord width;
    rc->GetWidth(cellText, width);

    nscoord totalTextWidth = width + bp.left + bp.right;
    if (totalTextWidth < remainWidth) {
      // If the text is not cropped, the text is smaller than the available 
      // space and we set the text rect to be that width. 
      textRect.width = totalTextWidth;
    }

    theRect = textRect;
  }
  
  float t2p = 0.0;
  mPresContext->GetTwipsToPixels(&t2p);
  
  *aX = NSToIntRound(theRect.x * t2p);
  *aY = NSToIntRound(theRect.y * t2p);
  *aWidth = NSToIntRound(theRect.width * t2p);
  *aHeight = NSToIntRound(theRect.height * t2p);
 
  return NS_OK;
}

nsresult
nsTreeBodyFrame::GetItemWithinCellAt(PRInt32 aX, const nsRect& aCellRect, 
                                         PRInt32 aRowIndex,
                                         nsTreeColumn* aColumn, PRUnichar** aChildElt)
{
  // Obtain the properties for our cell.
  PrefillPropertyArray(aRowIndex, aColumn);
  mView->GetCellProperties(aRowIndex, aColumn->GetID().get(), mScratchArray);

  // Resolve style for the cell.
  nsCOMPtr<nsIStyleContext> cellContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell, getter_AddRefs(cellContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  const nsStyleMargin* cellMarginData = (const nsStyleMargin*)cellContext->GetStyleData(eStyleStruct_Margin);
  nsMargin cellMargin;
  cellMarginData->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  if (aX < cellRect.x || aX >= cellRect.x + cellRect.width) {
    // The user clicked within the cell's margins/borders/padding.  This constitutes a click on the cell.
    *aChildElt = ToNewUnicode(NS_LITERAL_STRING("cell"));
    return NS_OK;
  }

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // XXX Handle right alignment hit testing.

  if (aColumn->IsPrimary()) {
    // If we're the primary column, we have indentation and a twisty.
    PRInt32 level;
    mView->GetLevel(aRowIndex, &level);

    currX += mIndentation*level;
    remainingWidth -= mIndentation*level;

    if (aX < currX) {
      // The user clicked within the indentation.
      *aChildElt = ToNewUnicode(NS_LITERAL_STRING("cell"));
      return NS_OK;
    }

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
    PRBool hasTwisty = PR_FALSE;
    PRBool isContainer = PR_FALSE;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      PRBool isContainerEmpty = PR_FALSE;
      mView->IsContainerEmpty(aRowIndex, &isContainerEmpty);
      if (!isContainerEmpty)
        hasTwisty = PR_TRUE;
    }

    // Resolve style for the twisty.
    nsCOMPtr<nsIStyleContext> twistyContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty, getter_AddRefs(twistyContext));

    // We will treat a click as hitting the twisty if it happens on the margins, borders, padding,
    // or content of the twisty object.  By allowing a "slop" into the margin, we make it a little
    // bit easier for a user to hit the twisty.  (We don't want to be too picky here.)
    nsRect imageSize = GetImageSize(aRowIndex, aColumn->GetID().get(), PR_TRUE, twistyContext);
    const nsStyleMargin* twistyMarginData = (const nsStyleMargin*)twistyContext->GetStyleData(eStyleStruct_Margin);
    nsMargin twistyMargin;
    twistyMarginData->GetMargin(twistyMargin);
    imageSize.Inflate(twistyMargin);
    twistyRect.width = imageSize.width;

    // Now we test to see if aX is actually within the twistyRect.  If it is, and if the item should
    // have a twisty, then we return "twisty".  If it is within the rect but we shouldn't have a twisty,
    // then we return "cell".
    if (aX >= twistyRect.x && aX < twistyRect.x + twistyRect.width) {
      if (hasTwisty)
        *aChildElt = ToNewUnicode(NS_LITERAL_STRING("twisty"));
      else
        *aChildElt = ToNewUnicode(NS_LITERAL_STRING("cell"));
      return NS_OK;
    }

    currX += twistyRect.width;
    remainingWidth -= twistyRect.width;    
  }
  
  // Now test to see if the user hit the icon for the cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  
  // Resolve style for the image.
  nsCOMPtr<nsIStyleContext> imageContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage, getter_AddRefs(imageContext));

  nsRect iconSize = GetImageSize(aRowIndex, aColumn->GetID().get(), PR_FALSE, imageContext);
  const nsStyleMargin* imageMarginData = (const nsStyleMargin*)imageContext->GetStyleData(eStyleStruct_Margin);
  nsMargin imageMargin;
  imageMarginData->GetMargin(imageMargin);
  iconSize.Inflate(imageMargin);
  iconRect.width = iconSize.width;

  if (aX >= iconRect.x && aX < iconRect.x + iconRect.width) {
    // The user clicked on the image.
    *aChildElt = ToNewUnicode(NS_LITERAL_STRING("image"));
    return NS_OK;
  }

  // Just assume "text".
  // XXX For marquee selection, we'll have to make this more precise and do text measurement.
  *aChildElt = ToNewUnicode(NS_LITERAL_STRING("text"));
  return NS_OK;
}

void
nsTreeBodyFrame::GetCellWidth(PRInt32 aRow, const nsAString& aColID,
                              nsIRenderingContext* aRenderingContext,
                              nscoord& aDesiredSize, nscoord& aCurrentSize)
{
  nsTreeColumn* currCol = nsnull;
  // Keep looping until we find a column with a matching Id.
  for (currCol = mColumns; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetID().Equals(aColID))
      break;
  }
  
  if (currCol) {
    // The rect for the current cell.
    nsRect cellRect(0, 0, currCol->GetWidth(), mRowHeight);

    // Adjust borders and padding for the cell.
    nsCOMPtr<nsIStyleContext> cellContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell, getter_AddRefs(cellContext));
    nsMargin m(0,0,0,0);
    nsStyleBorderPadding  bPad;
    cellContext->GetBorderPaddingFor(bPad);
    bPad.GetBorderPadding(m);

    aCurrentSize = cellRect.width;
    aDesiredSize = m.left + m.right;

    if (currCol->IsPrimary()) {
      // If the current Column is a Primary, then we need to take into account 
      // the indentation and possibly a twisty. 

      // The amount of indentation is the indentation width (|mIndentation|) by the level.
      PRInt32 level;
      mView->GetLevel(aRow, &level);
      aDesiredSize += mIndentation * level;
      
      // Find the twisty rect by computing its size.
      nsCOMPtr<nsIStyleContext> twistyContext;
      GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty, getter_AddRefs(twistyContext));

      // |GetImageSize| returns the rect of the twisty image, including the 
      // borders and padding.
      nsRect twistyImageRect = GetImageSize(aRow, currCol->GetID().get(), PR_TRUE, twistyContext);
      
      // Add in the margins of the twisty element.
      const nsStyleMargin* twistyMarginData = (const nsStyleMargin*) twistyContext->GetStyleData(eStyleStruct_Margin);
      nsMargin twistyMargin;
      twistyMarginData->GetMargin(twistyMargin);
      twistyImageRect.Inflate(twistyMargin);

      aDesiredSize += twistyImageRect.width;
    }

    nsCOMPtr<nsIStyleContext> imageContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage, getter_AddRefs(imageContext));

    // Account for the width of the cell image.
    nsRect imageSize = GetImageSize(aRow, currCol->GetID().get(), PR_FALSE, imageContext);
    aDesiredSize += imageSize.width;
    
    // Get the cell text.
    nsAutoString cellText;
    mView->GetCellText(aRow, currCol->GetID().get(), cellText);

    nsCOMPtr<nsIStyleContext> textContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext, getter_AddRefs(textContext));

    // Get the borders and padding for the text.
    nsStyleBorderPadding borderPadding;
    textContext->GetBorderPaddingFor(borderPadding);
    nsMargin bp(0,0,0,0);
    borderPadding.GetBorderPadding(bp);
    
    // Get the font style for the text and pass it to the rendering context.
    const nsStyleFont* fontStyle = (const nsStyleFont*) textContext->GetStyleData(eStyleStruct_Font);
    aRenderingContext->SetFont(fontStyle->mFont, nsnull);

    // Get the width of the text itself
    nscoord width;
    aRenderingContext->GetWidth(cellText, width);
    nscoord totalTextWidth = width + bp.left + bp.right;
    aDesiredSize += totalTextWidth;
  }
}

NS_IMETHODIMP
nsTreeBodyFrame::IsCellCropped(PRInt32 aRow, const nsAString& aColID, PRBool *_retval)
{  
  nscoord currentSize, desiredSize;
  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIRenderingContext> rc;
  presShell->CreateRenderingContext(this, getter_AddRefs(rc));

  GetCellWidth(aRow, aColID, rc, desiredSize, currentSize);
  *_retval = desiredSize > currentSize;
  
  return NS_OK;
}

void
nsTreeBodyFrame::MarkDirtyIfSelect()
{
  nsCOMPtr<nsIContent> baseElement;
  GetBaseElement(getter_AddRefs(baseElement));
  nsCOMPtr<nsIAtom> tag;
  baseElement->GetTag(*getter_AddRefs(tag));
  if (tag == nsHTMLAtoms::select) {
    // If we are an intrinsically sized select widget, we may need to
    // resize, if the widest item was removed or a new item was added.
    // XXX optimize this more

    mStringWidth = -1;
    nsBoxLayoutState state(mPresContext);
    MarkDirty(state);
  }
}

NS_IMETHODIMP nsTreeBodyFrame::RowCountChanged(PRInt32 aIndex, PRInt32 aCount)
{
  if (aCount == 0 || !mView)
    return NS_OK; // Nothing to do.

  PRInt32 count = PR_ABS(aCount);
  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);

  // Adjust our selection.
  nsCOMPtr<nsITreeSelection> sel;
  mView->GetSelection(getter_AddRefs(sel));
  if (sel)
    sel->AdjustSelection(aIndex, aCount);

  PRInt32 last;
  GetLastVisibleRow(&last);
  if (aIndex >= mTopRowIndex && aIndex <= last)
    InvalidateRange(aIndex, last);

  if (mTopRowIndex == 0) {    
    // Just update the scrollbar and return.
    InvalidateScrollbar();
    CheckVerticalOverflow();
    MarkDirtyIfSelect();
    return NS_OK;
  }

  // Adjust our top row index.
  if (aCount > 0) {
    if (mTopRowIndex > aIndex) {
      // Rows came in above us.  Augment the top row index.
      mTopRowIndex += aCount;
      UpdateScrollbar();
    }
  }
  else if (aCount < 0) {
    if (mTopRowIndex > aIndex+count-1) {
      // No need to invalidate. The remove happened
      // completely above us (offscreen).
      mTopRowIndex -= count;
      UpdateScrollbar();
    }
    else if (mTopRowIndex >= aIndex) {
      // This is a full-blown invalidate.
      if (mTopRowIndex + mPageCount > rowCount - 1)
        mTopRowIndex = PR_MAX(0, rowCount - 1 - mPageCount);
      UpdateScrollbar();
      Invalidate();
    }
  }

  InvalidateScrollbar();
  CheckVerticalOverflow();
  MarkDirtyIfSelect();

  return NS_OK;
}

void
nsTreeBodyFrame::PrefillPropertyArray(PRInt32 aRowIndex, nsTreeColumn* aCol)
{
  mScratchArray->Clear();
  
  // focus
  if (mFocused)
    mScratchArray->AppendElement(nsXULAtoms::focus);

  // sort
  PRBool sorted = PR_FALSE;
  mView->IsSorted(&sorted);
  if (sorted)
    mScratchArray->AppendElement(nsXULAtoms::sorted);

  // drag session
  if (mDragSession)
    mScratchArray->AppendElement(nsXULAtoms::dragSession);

  if (aRowIndex != -1) {
    nsCOMPtr<nsITreeSelection> selection;
    mView->GetSelection(getter_AddRefs(selection));
  
    if (selection) {
      // selected
      PRBool isSelected;
      selection->IsSelected(aRowIndex, &isSelected);
      if (isSelected)
        mScratchArray->AppendElement(nsHTMLAtoms::selected);

      // current
      PRInt32 currentIndex;
      selection->GetCurrentIndex(&currentIndex);
      if (aRowIndex == currentIndex)
        mScratchArray->AppendElement(nsXULAtoms::current);
    }

    // container or leaf
    PRBool isContainer = PR_FALSE;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      mScratchArray->AppendElement(nsXULAtoms::container);

      // open or closed
      PRBool isOpen = PR_FALSE;
      mView->IsContainerOpen(aRowIndex, &isOpen);
      if (isOpen)
        mScratchArray->AppendElement(nsXULAtoms::open);
      else
        mScratchArray->AppendElement(nsXULAtoms::closed);
    }
    else {
      mScratchArray->AppendElement(nsXULAtoms::leaf);
    }

    // drop orientation
    if (mDropAllowed && mDropRow == aRowIndex) {
      if (mDropOrient == nsITreeView::inDropBefore)
        mScratchArray->AppendElement(nsXULAtoms::dropBefore);
      else if (mDropOrient == nsITreeView::inDropOn)
        mScratchArray->AppendElement(nsXULAtoms::dropOn);
      else if (mDropOrient == nsITreeView::inDropAfter)
        mScratchArray->AppendElement(nsXULAtoms::dropAfter);
    }
  }

  if (aCol) {
    nsCOMPtr<nsIAtom> colID;
    aCol->GetIDAtom(getter_AddRefs(colID));
    mScratchArray->AppendElement(colID);

    if (aCol->IsPrimary())
      mScratchArray->AppendElement(nsXULAtoms::primary);

    if (aCol->GetType() == nsTreeColumn::eProgressMeter) {
      mScratchArray->AppendElement(nsXULAtoms::progressmeter);

      PRInt32 state = nsITreeView::progressNone;
      if (aRowIndex != -1)
        mView->GetProgressMode(aRowIndex, aCol->GetID().get(), &state);
      if (state == nsITreeView::progressNormal)
        mScratchArray->AppendElement(nsXULAtoms::progressNormal);
      else if (state == nsITreeView::progressUndetermined)
        mScratchArray->AppendElement(nsXULAtoms::progressUndetermined);
      else if (state == nsITreeView::progressNone)
        mScratchArray->AppendElement(nsXULAtoms::progressNone);
    }
  }
}

nsresult
nsTreeBodyFrame::GetImage(PRInt32 aRowIndex, const PRUnichar* aColID, PRBool aUseContext,
                          nsIStyleContext* aStyleContext, PRBool& aAllowImageRegions, imgIContainer** aResult)
{
  *aResult = nsnull;

  const nsAString* imagePtr;
  nsAutoString imageSrc;
  mView->GetImageSrc(aRowIndex, aColID, imageSrc);
  if (!aUseContext && imageSrc.Length() > 0) {
    imagePtr = &imageSrc;
    aAllowImageRegions = PR_FALSE;
  }
  else {
    // Obtain the URL from the style context.
    aAllowImageRegions = PR_TRUE;
    const nsStyleList* myList =
      (const nsStyleList*)aStyleContext->GetStyleData(eStyleStruct_List);
    if (myList->mListStyleImage.Length() > 0)
      imagePtr = &myList->mListStyleImage;
    else
      return NS_OK;
  }
  nsStringKey key(*imagePtr);

  if (mImageCache) {
    // Look the image up in our cache.
    nsCOMPtr<imgIRequest> imgReq = getter_AddRefs(NS_STATIC_CAST(imgIRequest*, mImageCache->Get(&key)));
    if (imgReq) {
      // Find out if the image has loaded.
      PRUint32 status;
      imgReq->GetImageStatus(&status);
      imgReq->GetImage(aResult); // We hand back the image here.  The GetImage call addrefs *aResult.
      PRUint32 numFrames = 1;
      if (*aResult)
        (*aResult)->GetNumFrames(&numFrames);

      if ((!(status & imgIRequest::STATUS_LOAD_COMPLETE)) || numFrames > 1) {
        // We either aren't done loading, or we're animating. Add our row as a listener for invalidations.
        nsCOMPtr<imgIDecoderObserver> obs;
        imgReq->GetDecoderObserver(getter_AddRefs(obs));
        nsCOMPtr<nsITreeImageListener> listener(do_QueryInterface(obs));
        if (listener)
          listener->AddRow(aRowIndex);
        return NS_OK;
      }
    }
  }

  if (!*aResult) {
    // Create a new nsTreeImageListener object and pass it our row and column
    // information.
    nsTreeImageListener* listener = new nsTreeImageListener(mTreeBoxObject, aColID);
    if (!listener)
      return NS_ERROR_OUT_OF_MEMORY;

    listener->AddRow(aRowIndex);

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    if (!doc)
      // The page is currently being torn down.  Why bother.
      return NS_ERROR_FAILURE;
    doc->GetBaseURL(*getter_AddRefs(baseURI));

    nsCOMPtr<nsIURI> srcURI;
    NS_NewURI(getter_AddRefs(srcURI), *imagePtr, nsnull, baseURI);
    if (!srcURI)
      return NS_ERROR_FAILURE;
    nsCOMPtr<imgIRequest> imageRequest;

    nsresult rv;
    nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
    if (NS_FAILED(rv))
      return rv;

    // Get the documment URI for the referrer.
    nsCOMPtr<nsIURI> documentURI;
    doc->GetDocumentURL(getter_AddRefs(documentURI));

    mImageGuard = PR_TRUE;
    // XXX: initialDocumentURI is NULL!
    rv = il->LoadImage(srcURI, nsnull, documentURI, nsnull, listener, mPresContext, nsIRequest::LOAD_NORMAL, nsnull, nsnull, getter_AddRefs(imageRequest));
    mImageGuard = PR_FALSE;

    if (!imageRequest)
      return NS_ERROR_FAILURE;

    // In a case it was already cached.
    imageRequest->GetImage(aResult);

    if (!mImageCache) {
      mImageCache = new nsSupportsHashtable(16);
      if (!mImageCache)
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mImageCache->Put(&key, imageRequest);
  }
  return NS_OK;
}

nsRect nsTreeBodyFrame::GetImageSize(PRInt32 aRowIndex, const PRUnichar* aColID, PRBool aUseContext,
                                         nsIStyleContext* aStyleContext)
{
  // XXX We should respond to visibility rules for collapsed vs. hidden.

  // This method returns the width of the twisty INCLUDING borders and padding.
  // It first checks the style context for a width.  If none is found, it tries to
  // use the default image width for the twisty.  If no image is found, it defaults
  // to border+padding.
  nsRect r(0,0,0,0);
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  aStyleContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(m);
  r.Inflate(m);

  // Now r contains our border+padding info.  We now need to get our width and
  // height.
  PRBool needWidth = PR_FALSE;
  PRBool needHeight = PR_FALSE;

  // We have to load image even though we already have a size.
  // Don't change this, otherwise things start to go crazy.
  PRBool useImageRegion = PR_TRUE;
  nsCOMPtr<imgIContainer> image;
  GetImage(aRowIndex, aColID, aUseContext, aStyleContext, useImageRegion, getter_AddRefs(image));

  const nsStylePosition* myPosition = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
  const nsStyleList* myList = (const nsStyleList*)
        aStyleContext->GetStyleData(eStyleStruct_List);

  if (useImageRegion) {
    r.x += myList->mImageRegion.x;
    r.y += myList->mImageRegion.y;
  }

  if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord)  {
    PRInt32 val = myPosition->mWidth.GetCoordValue();
    r.width += val;
  }
  else if (useImageRegion && myList->mImageRegion.width > 0)
    r.width += myList->mImageRegion.width;
  else 
    needWidth = PR_TRUE;

  if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
    PRInt32 val = myPosition->mHeight.GetCoordValue();
    r.height += val;
  }
  else if (useImageRegion && myList->mImageRegion.height > 0)
    r.height += myList->mImageRegion.height;
  else 
    needHeight = PR_TRUE;

  if (image) {
    if (needWidth || needHeight) {
      // Get the natural image size.
      float p2t;
      mPresContext->GetPixelsToTwips(&p2t);

      if (needWidth) {
        // Get the size from the image.
        nscoord width;
        image->GetWidth(&width);
        r.width += NSIntPixelsToTwips(width, p2t); 
      }
    
      if (needHeight) {
        nscoord height;
        image->GetHeight(&height);
        r.height += NSIntPixelsToTwips(height, p2t); 
      }
    }
  }

  return r;
}

PRInt32 nsTreeBodyFrame::GetRowHeight()
{
  // Look up the correct height.  It is equal to the specified height
  // + the specified margins.
  nsCOMPtr<nsIStyleContext> rowContext;
  mScratchArray->Clear();
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow, getter_AddRefs(rowContext));
  if (rowContext) {
    const nsStylePosition* myPosition = (const nsStylePosition*)
          rowContext->GetStyleData(eStyleStruct_Position);

    nscoord minHeight = 0;
    if (myPosition->mMinHeight.GetUnit() == eStyleUnit_Coord)
      minHeight = myPosition->mMinHeight.GetCoordValue();

    nscoord height = 0;
    if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)
      height = myPosition->mHeight.GetCoordValue();

    if (height < minHeight)
      height = minHeight;

    if (height > 0) {
      float t2p;
      mPresContext->GetTwipsToPixels(&t2p);
      height = NSTwipsToIntPixels(height, t2p);
      height += height % 2;
      float p2t;
      mPresContext->GetPixelsToTwips(&p2t);
      height = NSIntPixelsToTwips(height, p2t);

      // XXX Check box-sizing to determine if border/padding should augment the height
      // Inflate the height by our margins.
      nsRect rowRect(0,0,0,height);
      const nsStyleMargin* rowMarginData = (const nsStyleMargin*)rowContext->GetStyleData(eStyleStruct_Margin);
      nsMargin rowMargin;
      rowMarginData->GetMargin(rowMargin);
      rowRect.Inflate(rowMargin);
      height = rowRect.height;
      return height;
    }
  }

  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  return NSIntPixelsToTwips(18, p2t); // As good a default as any.
}

PRInt32 nsTreeBodyFrame::GetIndentation()
{
  // Look up the correct indentation.  It is equal to the specified indentation width.
  nsCOMPtr<nsIStyleContext> indentContext;
  mScratchArray->Clear();
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreeindentation, getter_AddRefs(indentContext));
  if (indentContext) {
    const nsStylePosition* myPosition = (const nsStylePosition*)
          indentContext->GetStyleData(eStyleStruct_Position);
    if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord)  {
      nscoord val = myPosition->mWidth.GetCoordValue();
      return val;
    }
  }
  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  return NSIntPixelsToTwips(16, p2t); // As good a default as any.
}

nsRect nsTreeBodyFrame::GetInnerBox()
{
  nsRect r(0,0,mRect.width, mRect.height);
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  mStyleContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(m);
  r.Deflate(m);
  return r;
}

nsLineStyle nsTreeBodyFrame::ConvertBorderStyleToLineStyle(PRUint8 aBorderStyle)
{
  switch (aBorderStyle) {
    case NS_STYLE_BORDER_STYLE_DOTTED:
      return nsLineStyle_kDotted;
    case NS_STYLE_BORDER_STYLE_DASHED:
      return nsLineStyle_kDashed;
    default:
      return nsLineStyle_kSolid;
  }
}

// Painting routines
NS_IMETHODIMP
nsTreeBodyFrame::Paint(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  // XXX This trap handles an odd bogus 1 pixel invalidation that we keep getting 
  // when scrolling.
  if (aDirtyRect.width == 1)
    return NS_OK;

  if (aWhichLayer != NS_FRAME_PAINT_LAYER_BACKGROUND &&
      aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND)
    return NS_OK;

  const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
  if (!vis->IsVisibleOrCollapsed())
    return NS_OK; // We're invisible.  Don't paint.

  // Handles painting our background, border, and outline.
  nsresult rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FAILED(rv)) return rv;

  if (!mView)
    return NS_OK;

  PRBool clipState = PR_FALSE;
  
  // Update our available height and our page count.
  mInnerBox = GetInnerBox();
  PRInt32 oldPageCount = mPageCount;
  if (!mHasFixedRowCount)
    mPageCount = mInnerBox.height/mRowHeight;

  if (oldPageCount != mPageCount) {
    // Schedule a ResizeReflow that will update our page count properly.
    nsBoxLayoutState state(mPresContext);
    MarkDirty(state);
  }

  PRInt32 rowCount = 0;
  mView->GetRowCount(&rowCount);
  
  // Ensure our column info is built.
  EnsureColumns();

  // Loop through our columns and paint them (e.g., for sorting).  This is only
  // relevant when painting backgrounds, since columns contain no content.  Content
  // is contained in the rows.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nscoord currX = mInnerBox.x;
    for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
         currCol = currCol->GetNext()) {
      nsRect colRect(currX, mInnerBox.y, currCol->GetWidth(), mInnerBox.height);
      PRInt32 overflow = colRect.x+colRect.width-(mInnerBox.x+mInnerBox.width);
      if (overflow > 0)
        colRect.width -= overflow;
      nsRect dirtyRect;
      if (dirtyRect.IntersectRect(aDirtyRect, colRect)) {
        PaintColumn(currCol, colRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer); 
      }
      currX += currCol->GetWidth();
    }
  }

  // Loop through our on-screen rows.
  for (PRInt32 i = mTopRowIndex; i < rowCount && i < mTopRowIndex+mPageCount+1; i++) {
    nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*(i-mTopRowIndex), mInnerBox.width, mRowHeight);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, rowRect) && rowRect.y < (mInnerBox.y+mInnerBox.height)) {
      PRBool clip = (rowRect.y + rowRect.height > mInnerBox.y + mInnerBox.height);
      if (clip) {
        // We need to clip the last row, since it extends outside our inner box.  Push
        // a clip rect down.
        PRInt32 overflow = (rowRect.y+rowRect.height) - (mInnerBox.y+mInnerBox.height);
        nsRect clipRect(rowRect.x, rowRect.y, mInnerBox.width, mRowHeight-overflow);
        aRenderingContext.PushState();
        aRenderingContext.SetClipRect(clipRect, nsClipCombine_kReplace, clipState);
      }

      PaintRow(i, rowRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

      if (clip)
        aRenderingContext.PopState(clipState);
    }
  }

  if (mDropAllowed)
    PaintDropFeedback(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer); 

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintColumn(nsTreeColumn*        aColumn,
                             const nsRect&        aColumnRect,
                             nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer)
{
  if (aColumnRect.width == 0)
    return NS_OK; // Don't paint hidden columns.

  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused, and the col ID.
  PrefillPropertyArray(-1, aColumn);
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aColumn->GetElement()));
  mView->GetColumnProperties(aColumn->GetID().get(), elt, mScratchArray);

  // Read special properties from attributes on the column content node
  nsAutoString attr;
  aColumn->GetElement()->GetAttr(kNameSpaceID_None, nsXULAtoms::insertbefore, attr);
  if (attr.Equals(NS_LITERAL_STRING("true")))
    mScratchArray->AppendElement(nsXULAtoms::insertbefore);
  attr.Assign(NS_LITERAL_STRING(""));
  aColumn->GetElement()->GetAttr(kNameSpaceID_None, nsXULAtoms::insertafter, attr);
  if (attr.Equals(NS_LITERAL_STRING("true")))
    mScratchArray->AppendElement(nsXULAtoms::insertafter);
  
  // Resolve style for the column.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> colContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreecolumn, getter_AddRefs(colContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect colRect(aColumnRect);
  const nsStyleMargin* colMarginData = (const nsStyleMargin*)colContext->GetStyleData(eStyleStruct_Margin);
  nsMargin colMargin;
  colMarginData->GetMargin(colMargin);
  colRect.Deflate(colMargin);

  PaintBackgroundLayer(colContext, aPresContext, aRenderingContext, colRect, aDirtyRect);

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintRow(PRInt32              aRowIndex,
                          const nsRect&        aRowRect,
                          nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer)
{
  // We have been given a rect for our row.  We treat this row like a full-blown
  // frame, meaning that it can have borders, margins, padding, and a background.
  
  // Without a view, we have no data. Check for this up front.
  if (!mView)
    return NS_OK;

  // Now obtain the properties for our row.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused
  PrefillPropertyArray(aRowIndex, nsnull);
  mView->GetRowProperties(aRowIndex, mScratchArray);

  // Resolve style for the row.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> rowContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow, getter_AddRefs(rowContext));

  // Obtain the margins for the row and then deflate our rect by that 
  // amount.  The row is assumed to be contained within the deflated rect.
  nsRect rowRect(aRowRect);
  const nsStyleMargin* rowMarginData = (const nsStyleMargin*)rowContext->GetStyleData(eStyleStruct_Margin);
  nsMargin rowMargin;
  rowMarginData->GetMargin(rowMargin);
  rowRect.Deflate(rowMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // row rect. If a -moz-appearance is provided, use theme drawing only if the current row
  // is not selected (since we draw the selection as part of drawing the background).
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    PRBool useTheme = PR_FALSE;
    nsCOMPtr<nsITheme> theme;
    const nsStyleDisplay* displayData = (const nsStyleDisplay*)rowContext->GetStyleData(eStyleStruct_Display);
    if ( displayData->mAppearance ) {
      aPresContext->GetTheme(getter_AddRefs(theme));
      if (theme && theme->ThemeSupportsWidget(aPresContext, nsnull, displayData->mAppearance))
        useTheme = PR_TRUE;
    }
    PRBool isSelected = PR_FALSE;
    nsCOMPtr<nsITreeSelection> selection;
    GetSelection(getter_AddRefs(selection));
    if ( selection ) 
      selection->IsSelected(aRowIndex, &isSelected);
    if ( useTheme && !isSelected )
      theme->DrawWidgetBackground(&aRenderingContext, this, 
                                    displayData->mAppearance, rowRect, aDirtyRect);     
    else
      PaintBackgroundLayer(rowContext, aPresContext, aRenderingContext, rowRect, aDirtyRect);
  }
  
  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(rowContext, rowRect);

  PRBool isSeparator = PR_FALSE;
  mView->IsSeparator(aRowIndex, &isSeparator);
  if (isSeparator) {
    // The row is a separator. Paint only a double horizontal line.

    // Resolve style for the separator.
    nsCOMPtr<nsIStyleContext> separatorContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreeseparator, getter_AddRefs(separatorContext));
    PRBool useTheme = PR_FALSE;
    nsCOMPtr<nsITheme> theme;
    const nsStyleDisplay* displayData = (const nsStyleDisplay*)separatorContext->GetStyleData(eStyleStruct_Display);
    if ( displayData->mAppearance ) {
      aPresContext->GetTheme(getter_AddRefs(theme));
      if (theme && theme->ThemeSupportsWidget(aPresContext, nsnull, displayData->mAppearance))
        useTheme = PR_TRUE;
    }

    // use -moz-appearance if provided.
    if ( useTheme ) 
      theme->DrawWidgetBackground(&aRenderingContext, this, 
                                    displayData->mAppearance, rowRect, aDirtyRect); 
    else {
      // Get border style
      const nsStyleBorder* borderStyle = (const nsStyleBorder*)separatorContext->GetStyleData(eStyleStruct_Border);

      aRenderingContext.PushState();

      PRUint8 side = NS_SIDE_TOP;
      nscoord currY = rowRect.y + rowRect.height / 2;
      for (PRInt32 i = 0; i < 2; i++) {
        nscolor color;
        PRBool transparent; PRBool foreground;
        borderStyle->GetBorderColor(side, color, transparent, foreground);
        aRenderingContext.SetColor(color);
        PRUint8 style;
        style = borderStyle->GetBorderStyle(side);
        aRenderingContext.SetLineStyle(ConvertBorderStyleToLineStyle(style));

        aRenderingContext.DrawLine(rowRect.x, currY, rowRect.x + rowRect.width, currY);

        side = NS_SIDE_BOTTOM;
        currY += 16;
      }

      PRBool clipState;
      aRenderingContext.PopState(clipState);
    }
  }
  else {
    // Now loop over our cells. Only paint a cell if it intersects with our dirty rect.
    nscoord currX = rowRect.x;
    for (nsTreeColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
         currCol = currCol->GetNext()) {
      nsRect cellRect(currX, rowRect.y, currCol->GetWidth(), rowRect.height);
      PRInt32 overflow = cellRect.x+cellRect.width-(mInnerBox.x+mInnerBox.width);
      if (overflow > 0)
        cellRect.width -= overflow;
      nsRect dirtyRect;
      if (dirtyRect.IntersectRect(aDirtyRect, cellRect))
        PaintCell(aRowIndex, currCol, cellRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer); 
      currX += currCol->GetWidth();
    }
  }

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintCell(PRInt32              aRowIndex,
                           nsTreeColumn*        aColumn,
                           const nsRect&        aCellRect,
                           nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsFramePaintLayer    aWhichLayer)
{
  if (aCellRect.width == 0)
    return NS_OK; // Don't paint cells in hidden columns.

  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused, and the col ID.
  PrefillPropertyArray(aRowIndex, aColumn);
  mView->GetCellProperties(aRowIndex, aColumn->GetID().get(), mScratchArray);

  // Resolve style for the cell.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> cellContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell, getter_AddRefs(cellContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  const nsStyleMargin* cellMarginData = (const nsStyleMargin*)cellContext->GetStyleData(eStyleStruct_Margin);
  nsMargin cellMargin;
  cellMarginData->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // row rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(cellContext, aPresContext, aRenderingContext, cellRect, aDirtyRect);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // Now we paint the contents of the cells.
  // Text alignment determines the order in which we paint.  
  // LEFT means paint from left to right.
  // RIGHT means paint from right to left.
  // XXX Implement RIGHT alignment!

  if (aColumn->IsPrimary()) {
    // If we're the primary column, we need to indent and paint the twisty and any connecting lines
    // between siblings.

    PRInt32 level;
    mView->GetLevel(aRowIndex, &level);

    currX += mIndentation * level;
    remainingWidth -= mIndentation * level;

    // Resolve the style to use for the connecting lines.
    nsCOMPtr<nsIStyleContext> lineContext;
    GetPseudoStyleContext(nsCSSAnonBoxes::moztreeline, getter_AddRefs(lineContext));
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)lineContext->GetStyleData(eStyleStruct_Visibility);
    
    if (vis->IsVisibleOrCollapsed() && level && NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      // Paint the connecting lines.

      // Get the size of the twisty. We don't want to paint the twisty
      // before painting of connecting lines since it would paint lines over
      // the twisty. But we need to leave a place for it.
      nsCOMPtr<nsIStyleContext> twistyContext;
      GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty, getter_AddRefs(twistyContext));

      nsRect twistySize = GetImageSize(aRowIndex, aColumn->GetID().get(), PR_TRUE, twistyContext);

      const nsStyleMargin* twistyMarginData = (const nsStyleMargin*)twistyContext->GetStyleData(eStyleStruct_Margin);
      nsMargin twistyMargin;
      twistyMarginData->GetMargin(twistyMargin);
      twistySize.Inflate(twistyMargin);

      aRenderingContext.PushState();

      const nsStyleBorder* borderStyle = (const nsStyleBorder*)lineContext->GetStyleData(eStyleStruct_Border);
      nscolor color;
      PRBool transparent; PRBool foreground;
      borderStyle->GetBorderColor(NS_SIDE_LEFT, color, transparent, foreground);

      aRenderingContext.SetColor(color);
      PRUint8 style;
      style = borderStyle->GetBorderStyle(NS_SIDE_LEFT);
      aRenderingContext.SetLineStyle(ConvertBorderStyleToLineStyle(style));

      nscoord lineX = currX;
      nscoord lineY = (aRowIndex - mTopRowIndex) * mRowHeight;

      // Compute the maximal level to paint.
      PRInt32 maxLevel = level;
      if (maxLevel > cellRect.width / mIndentation)
        maxLevel = cellRect.width / mIndentation;

      PRInt32 currentParent = aRowIndex;
      for (PRInt32 i = level; i > 0; i--) {
        if (i <= maxLevel) {
          lineX = currX + twistySize.width + mIndentation / 2;

          nscoord srcX = lineX - (level - i + 1) * mIndentation;
          if (srcX <= cellRect.x + cellRect.width) {
            // Paint full vertical line only if we have next sibling.
            PRBool hasNextSibling;
            mView->HasNextSibling(currentParent, aRowIndex, &hasNextSibling);
            if (hasNextSibling)
              aRenderingContext.DrawLine(srcX, lineY, srcX, lineY + mRowHeight);
            else if (i == level)
              aRenderingContext.DrawLine(srcX, lineY, srcX, lineY + mRowHeight / 2);
          }
        }

        PRInt32 parent;
        mView->GetParentIndex(currentParent, &parent);
        if (parent == -1)
          break;
        currentParent = parent;
      }

      // Don't paint off our cell.
      if (level == maxLevel) {
        nscoord srcX = lineX - mIndentation + 16;
        if (srcX <= cellRect.x + cellRect.width) {
          nscoord destX = lineX - mIndentation / 2;
          if (destX > cellRect.x + cellRect.width)
            destX = cellRect.x + cellRect.width;
          aRenderingContext.DrawLine(srcX, lineY + mRowHeight / 2, destX, lineY + mRowHeight / 2);
        }
      }

      PRBool clipState;
      aRenderingContext.PopState(clipState);
    }

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
    PaintTwisty(aRowIndex, aColumn, twistyRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer,
                remainingWidth, currX);
  }
  
  // Now paint the icon for our cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  nsRect dirtyRect;
  if (dirtyRect.IntersectRect(aDirtyRect, iconRect))
    PaintImage(aRowIndex, aColumn, iconRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer,
               remainingWidth, currX);

  // Now paint our element, but only if we aren't a cycler column.
  // XXX until we have the ability to load images, allow the view to 
  // insert text into cycler columns...
  if (!aColumn->IsCycler()) {
    nsRect elementRect(currX, cellRect.y, remainingWidth, cellRect.height);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, elementRect)) {
      switch (aColumn->GetType()) {
        case nsTreeColumn::eText:
          PaintText(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
          break;
        case nsTreeColumn::eCheckbox:
          PaintCheckbox(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
          break;
        case nsTreeColumn::eProgressMeter:
          PRInt32 state;
          mView->GetProgressMode(aRowIndex, aColumn->GetID().get(), &state);
          switch (state) {
            case nsITreeView::progressNormal:
            case nsITreeView::progressUndetermined:
              PaintProgressMeter(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
              break;
            case nsITreeView::progressNone:
            default:
              PaintText(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
              break;
          }
          break;
      }
    }
  }

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintTwisty(PRInt32              aRowIndex,
                             nsTreeColumn*        aColumn,
                             const nsRect&        aTwistyRect,
                             nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             nscoord&             aRemainingWidth,
                             nscoord&             aCurrX)
{
  // Paint the twisty, but only if we are a non-empty container.
  PRBool shouldPaint = PR_FALSE;
  PRBool isContainer = PR_FALSE;
  mView->IsContainer(aRowIndex, &isContainer);
  if (isContainer) {
    PRBool isContainerEmpty = PR_FALSE;
    mView->IsContainerEmpty(aRowIndex, &isContainerEmpty);
    if (!isContainerEmpty)
      shouldPaint = PR_TRUE;
  }

  // Resolve style for the twisty.
  nsCOMPtr<nsIStyleContext> twistyContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty, getter_AddRefs(twistyContext));

  PRBool useTheme = PR_FALSE;
  nsCOMPtr<nsITheme> theme;
  const nsStyleDisplay* twistyDisplayData = (const nsStyleDisplay*)twistyContext->GetStyleData(eStyleStruct_Display);
  if ( twistyDisplayData->mAppearance ) {
    aPresContext->GetTheme(getter_AddRefs(theme));
    if (theme && theme->ThemeSupportsWidget(aPresContext, nsnull, twistyDisplayData->mAppearance))
      useTheme = PR_TRUE;
  }
  
  // Obtain the margins for the twisty and then deflate our rect by that 
  // amount.  The twisty is assumed to be contained within the deflated rect.
  nsRect twistyRect(aTwistyRect);
  const nsStyleMargin* twistyMarginData = (const nsStyleMargin*)twistyContext->GetStyleData(eStyleStruct_Margin);
  nsMargin twistyMargin;
  twistyMarginData->GetMargin(twistyMargin);
  twistyRect.Deflate(twistyMargin);

  // The twisty rect extends all the way to the end of the cell.  This is incorrect.  We need to
  // determine the twisty rect's true width.  This is done by examining the style context for
  // a width first.  If it has one, we use that.  If it doesn't, we use the image's natural width.
  // If the image hasn't loaded and if no width is specified, then we just bail. If there is
  // a -moz-apperance involved, adjust the rect by the minimum widget size provided by
  // the theme implementation.
  nsRect imageSize = GetImageSize(aRowIndex, aColumn->GetID().get(), PR_TRUE, twistyContext);
  if (imageSize.height > twistyRect.height)
    imageSize.height = twistyRect.height;
  if (imageSize.width > twistyRect.width)
    imageSize.width = twistyRect.width;
  else
    twistyRect.width = imageSize.width;
  if ( useTheme ) {
    nsSize minTwistySize(0,0);
    PRBool canOverride = PR_TRUE;
    theme->GetMinimumWidgetSize(&aRenderingContext, this, twistyDisplayData->mAppearance, &minTwistySize, &canOverride);
    
    // GMWS() returns size in pixels, we need to convert it back to twips
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    minTwistySize.width = NSIntPixelsToTwips(minTwistySize.width, p2t);
    minTwistySize.height = NSIntPixelsToTwips(minTwistySize.height, p2t);

    if ( twistyRect.width < minTwistySize.width || !canOverride )
      twistyRect.width = minTwistySize.width;
  }
  
  // Subtract out the remaining width.  This is done even when we don't actually paint a twisty in 
  // this cell, so that cells in different rows still line up.
  nsRect copyRect(twistyRect);
  copyRect.Inflate(twistyMargin);
  aRemainingWidth -= copyRect.width;
  aCurrX += copyRect.width;

  if (shouldPaint) {
    // If the layer is the background layer, we must paint our borders and background for our
    // image rect.
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
      PaintBackgroundLayer(twistyContext, aPresContext, aRenderingContext, twistyRect, aDirtyRect);
    else if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      if ( useTheme ) {
        // yeah, i know it says we're drawing a background, but a twisty is really a fg
        // object since it doesn't have anything that gecko would want to draw over it. Besides,
        // we have to prevent imagelib from drawing it.
        theme->DrawWidgetBackground(&aRenderingContext, this, 
                                      twistyDisplayData->mAppearance, twistyRect, aDirtyRect);
      }
      else {
        // Time to paint the twisty.
        // Adjust the rect for its border and padding.
        AdjustForBorderPadding(twistyContext, twistyRect);
        AdjustForBorderPadding(twistyContext, imageSize);

        // Get the image for drawing.
        nsCOMPtr<imgIContainer> image;
        PRBool useImageRegion = PR_TRUE;
        GetImage(aRowIndex, aColumn->GetID().get(), PR_TRUE, twistyContext, useImageRegion, getter_AddRefs(image));
        if (image) {
          nsPoint p(twistyRect.x, twistyRect.y);
          
          // Center the image. XXX Obey vertical-align style prop?
          if (imageSize.height < twistyRect.height) {
            p.y += (twistyRect.height - imageSize.height)/2;
          }
          
          // Paint the image.
          aRenderingContext.DrawImage(image, &imageSize, &p);
        }
      }        
    }
  }

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintImage(PRInt32              aRowIndex,
                            nsTreeColumn*        aColumn,
                            const nsRect&        aImageRect,
                            nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            nscoord&             aRemainingWidth,
                            nscoord&             aCurrX)
{
  // Resolve style for the image.
  nsCOMPtr<nsIStyleContext> imageContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage, getter_AddRefs(imageContext));

  // Obtain the margins for the twisty and then deflate our rect by that 
  // amount.  The twisty is assumed to be contained within the deflated rect.
  nsRect imageRect(aImageRect);
  const nsStyleMargin* imageMarginData = (const nsStyleMargin*)imageContext->GetStyleData(eStyleStruct_Margin);
  nsMargin imageMargin;
  imageMarginData->GetMargin(imageMargin);
  imageRect.Deflate(imageMargin);

  // If the column isn't a cycler, the image rect extends all the way to the end of the cell.  
  // This is incorrect.  We need to determine the image rect's true width.  This is done by 
  // examining the style context for a width first.  If it has one, we use that.  If it doesn't, 
  // we use the image's natural width.
  // If the image hasn't loaded and if no width is specified, then we just bail.
  nsRect imageSize = GetImageSize(aRowIndex, aColumn->GetID().get(), PR_FALSE, imageContext);

  if (imageSize.height > imageRect.height)
    imageSize.height = imageRect.height;
  if (imageSize.width > imageRect.width)
    imageSize.width = imageRect.width;
  else if (!aColumn->IsCycler())
    imageRect.width = imageSize.width;

  // Subtract out the remaining width.
  nsRect copyRect(imageRect);
  copyRect.Inflate(imageMargin);
  aRemainingWidth -= copyRect.width;
  aCurrX += copyRect.width;

  // If the layer is the background layer, we must paint our borders and background for our
  // image rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(imageContext, aPresContext, aRenderingContext, imageRect, aDirtyRect);
  else if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // Time to paint the twisty.
    // Adjust the rect for its border and padding.
    AdjustForBorderPadding(imageContext, imageRect);
    AdjustForBorderPadding(imageContext, imageSize);

    // Get the image for drawing.
    PRBool useImageRegion = PR_TRUE;
    nsCOMPtr<imgIContainer> image; 
    GetImage(aRowIndex, aColumn->GetID().get(), PR_FALSE, imageContext, useImageRegion, getter_AddRefs(image));
    if (image) {
      nsPoint p(imageRect.x, imageRect.y);
      
      // Center the image. XXX Obey vertical-align style prop?

      float t2p, p2t;
      mPresContext->GetTwipsToPixels(&t2p);
      mPresContext->GetPixelsToTwips(&p2t);

      if (imageSize.height < imageRect.height) {
        p.y += (imageRect.height - imageSize.height)/2;
      }

      // For cyclers, we also want to center the image in the column.
      if (aColumn->IsCycler() && imageSize.width < imageRect.width) {
        p.x += (imageRect.width - imageSize.width)/2;
      }

      // Paint the image.
      aRenderingContext.DrawImage(image, &imageSize, &p);
    }
  }

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintText(PRInt32              aRowIndex,
                           nsTreeColumn*        aColumn,
                           const nsRect&        aTextRect,
                           nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsFramePaintLayer    aWhichLayer)
{
  // Now obtain the text for our cell.
  nsAutoString text;
  mView->GetCellText(aRowIndex, aColumn->GetID().get(), text);

  if (text.Length() == 0)
    return NS_OK; // Don't paint an empty string. XXX What about background/borders? Still paint?

  // Resolve style for the text.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> textContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext, getter_AddRefs(textContext));

  // Obtain the margins for the text and then deflate our rect by that 
  // amount.  The text is assumed to be contained within the deflated rect.
  nsRect textRect(aTextRect);
  const nsStyleMargin* textMarginData = (const nsStyleMargin*)textContext->GetStyleData(eStyleStruct_Margin);
  nsMargin textMargin;
  textMarginData->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  // Compute our text size.
  const nsStyleFont* fontStyle = (const nsStyleFont*)textContext->GetStyleData(eStyleStruct_Font);

  nsCOMPtr<nsIDeviceContext> deviceContext;
  aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

  nsCOMPtr<nsIFontMetrics> fontMet;
  deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
  nscoord height, baseline;
  fontMet->GetHeight(height);
  fontMet->GetMaxAscent(baseline);

  // Center the text. XXX Obey vertical-align style prop?
  if (height < textRect.height) {
    textRect.y += (textRect.height - height)/2;
    textRect.height = height;
  }

  // Set our font.
  aRenderingContext.SetFont(fontMet);

  nscoord width;
  aRenderingContext.GetWidth(text, width);

  if (width > textRect.width) {
    // See if the width is even smaller than the ellipsis
    // If so, clear the text completely.
    nscoord ellipsisWidth;
    aRenderingContext.GetWidth(ELLIPSIS, ellipsisWidth);

    nscoord width = textRect.width;
    if (ellipsisWidth > width)
      text.SetLength(0);
    else if (ellipsisWidth == width)
      text.Assign(NS_LITERAL_STRING(ELLIPSIS));
    else {
      // We will be drawing an ellipsis, thank you very much.
      // Subtract out the required width of the ellipsis.
      // This is the total remaining width we have to play with.
      width -= ellipsisWidth;

      // Now we crop.
      switch (aColumn->GetCropStyle()) {
        default:
        case 0: {
          // Crop right. 
          nscoord cwidth;
          nscoord twidth = 0;
          int length = text.Length();
          int i;
          for (i = 0; i < length; ++i) {
            PRUnichar ch = text[i];
            aRenderingContext.GetWidth(ch,cwidth);
            if (twidth + cwidth > width)
              break;
            twidth += cwidth;
          }
          text.Truncate(i);
          text += NS_LITERAL_STRING(ELLIPSIS);
        }
        break;

        case 2: {
          // Crop left.
          nscoord cwidth;
          nscoord twidth = 0;
          int length = text.Length();
          int i;
          for (i=length-1; i >= 0; --i) {
            PRUnichar ch = text[i];
            aRenderingContext.GetWidth(ch,cwidth);
            if (twidth + cwidth > width)
              break;
            twidth += cwidth;
          }

          nsAutoString copy;
          text.Right(copy, length-1-i);
          text.Assign(NS_LITERAL_STRING(ELLIPSIS));
          text += copy;
        }
        break;

        case 1:
        {
          // Crop center.
          nsAutoString leftStr, rightStr;
          nscoord cwidth, twidth = 0;
          int length = text.Length();
          int rightPos = length - 1;
          for (int leftPos = 0; leftPos < rightPos; ++leftPos) {
            PRUnichar ch = text[leftPos];
            aRenderingContext.GetWidth(ch, cwidth);
            twidth += cwidth;
            if (twidth > width)
              break;
            leftStr.Append(ch);

            ch = text[rightPos];
            aRenderingContext.GetWidth(ch, cwidth);
            twidth += cwidth;
            if (twidth > width)
              break;
            rightStr.Insert(ch, 0);
            --rightPos;
          }
          text = leftStr + NS_LITERAL_STRING(ELLIPSIS) + rightStr;
        }
        break;
      }
    }
  }
  else {
    switch (aColumn->GetTextAlignment()) {
      case NS_STYLE_TEXT_ALIGN_RIGHT: {
        textRect.x += textRect.width - width;
      }
      break;
      case NS_STYLE_TEXT_ALIGN_CENTER: {
        textRect.x += (textRect.width - width) / 2;
      }
      break;
    }
  }

  aRenderingContext.GetWidth(text, width);
  textRect.width = width;

  // If the layer is the background layer, we must paint our borders and background for our
  // text rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(textContext, aPresContext, aRenderingContext, textRect, aDirtyRect);
  else if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // Time to paint our text. 
    // Adjust the rect for its border and padding.
    AdjustForBorderPadding(textContext, textRect);

    // Set our color.
    const nsStyleColor* colorStyle = (const nsStyleColor*)textContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(colorStyle->mColor);

    // Draw decorations.
    const nsStyleTextReset* textStyle = (const nsStyleTextReset*)textContext->GetStyleData(eStyleStruct_TextReset);
    PRUint8 decorations = textStyle->mTextDecoration;

    nscoord offset;
    nscoord size;
    if (decorations & (NS_FONT_DECORATION_OVERLINE | NS_FONT_DECORATION_UNDERLINE)) {
      fontMet->GetUnderline(offset, size);
      if (decorations & NS_FONT_DECORATION_OVERLINE)
        aRenderingContext.FillRect(textRect.x, textRect.y, width, size);
      if (decorations & NS_FONT_DECORATION_UNDERLINE)
        aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, width, size);
    }
    if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
      fontMet->GetStrikeout(offset, size);
      aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, width, size);
    }
#ifdef MOZ_TIMELINE
    NS_TIMELINE_START_TIMER("Render Outline Text");
#endif
#ifdef IBMBIDI
    nsresult rv = NS_ERROR_FAILURE;
    nsBidiPresUtils* bidiUtils;
    aPresContext->GetBidiUtils(&bidiUtils);
    
    if (bidiUtils) {
      const nsStyleVisibility* vis;
      GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&) vis);
      nsBidiDirection direction = 
        (NS_STYLE_DIRECTION_RTL == vis->mDirection) ?
        NSBIDI_RTL : NSBIDI_LTR;
      PRUnichar* buffer = (PRUnichar*) text.get();
      rv = bidiUtils->RenderText(buffer, text.Length(), direction,
                                 aPresContext, aRenderingContext,
                                 textRect.x, textRect.y + baseline);
    }
    if (NS_FAILED(rv))
#endif // IBMBIDI
    aRenderingContext.DrawString(text, textRect.x, textRect.y + baseline);
  }
#ifdef MOZ_TIMELINE
    NS_TIMELINE_STOP_TIMER("Render Outline Text");
    NS_TIMELINE_MARK_TIMER("Render Outline Text");
#endif

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintCheckbox(PRInt32              aRowIndex,
                               nsTreeColumn*        aColumn,
                               const nsRect&        aCheckboxRect,
                               nsIPresContext*      aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               const nsRect&        aDirtyRect,
                               nsFramePaintLayer    aWhichLayer)
{
  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintProgressMeter(PRInt32              aRowIndex,
                                    nsTreeColumn*        aColumn,
                                    const nsRect&        aProgressMeterRect,
                                    nsIPresContext*      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    const nsRect&        aDirtyRect,
                                    nsFramePaintLayer    aWhichLayer)
{

  // Resolve style for the progress meter.  It contains all the info we need
  // to lay ourselves out and to paint.
  nsCOMPtr<nsIStyleContext> meterContext;
  GetPseudoStyleContext(nsCSSAnonBoxes::moztreeprogressmeter, getter_AddRefs(meterContext));

  // Obtain the margins for the progress meter and then deflate our rect by that 
  // amount. The progress meter is assumed to be contained within the deflated
  // rect.
  nsRect meterRect(aProgressMeterRect);
  const nsStyleMargin* meterMarginData = (const nsStyleMargin*)meterContext->GetStyleData(eStyleStruct_Margin);
  nsMargin meterMargin;
  meterMarginData->GetMargin(meterMargin);
  meterRect.Deflate(meterMargin);

  // If the layer is the background layer, we must paint our borders and
  // background for our progress meter rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(meterContext, aPresContext, aRenderingContext, meterRect, aDirtyRect);
  else {
    // Time to paint our progress. 
    PRInt32 state;
    mView->GetProgressMode(aRowIndex, aColumn->GetID().get(), &state);
    if (state == nsITreeView::progressNormal) {
      // Adjust the rect for its border and padding.
      AdjustForBorderPadding(meterContext, meterRect);

      // Set our color.
      const nsStyleColor* colorStyle = (const nsStyleColor*)meterContext->GetStyleData(eStyleStruct_Color);
      aRenderingContext.SetColor(colorStyle->mColor);

      // Now obtain the value for our cell.
      nsAutoString value;
      mView->GetCellValue(aRowIndex, aColumn->GetID().get(), value);

      PRInt32 rv;
      PRInt32 intValue = value.ToInteger(&rv);
      if (intValue < 0)
        intValue = 0;
      else if (intValue > 100)
        intValue = 100;

      meterRect.width = NSToCoordRound((float)intValue / 100 * meterRect.width);
      PRBool useImageRegion = PR_TRUE;
      nsCOMPtr<imgIContainer> image;
      GetImage(aRowIndex, aColumn->GetID().get(), PR_TRUE, meterContext, useImageRegion, getter_AddRefs(image));
      if (image)
        aRenderingContext.DrawTile(image, 0, 0, &meterRect);
      else
        aRenderingContext.FillRect(meterRect);
    }
    else if (state == nsITreeView::progressUndetermined) {
      // Adjust the rect for its border and padding.
      AdjustForBorderPadding(meterContext, meterRect);

      PRBool useImageRegion = PR_TRUE;
      nsCOMPtr<imgIContainer> image;
      GetImage(aRowIndex, aColumn->GetID().get(), PR_TRUE, meterContext, useImageRegion, getter_AddRefs(image));
      if (image)
        aRenderingContext.DrawTile(image, 0, 0, &meterRect);
    }
  }

  return NS_OK;
}


nsresult
nsTreeBodyFrame::PaintDropFeedback(nsIPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect&        aDirtyRect,
                                   nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    if (mDropOrient == nsITreeView::inDropBefore ||
        mDropOrient == nsITreeView::inDropAfter) {
      // Paint the drop feedback in between rows.

      nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*(mDropRow-mTopRowIndex), mInnerBox.width, mRowHeight);
      // Paint the drop feedback at the same position for both orientations.
      if (mDropOrient == nsITreeView::inDropAfter)
        rowRect.y += mRowHeight;

      nsRect dirtyRect;
      if (dirtyRect.IntersectRect(aDirtyRect, rowRect)) {
        // Find the primary cell.
        nscoord currX = rowRect.x;
        nsTreeColumn* currCol;
        for (currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width;
             currCol = currCol->GetNext()) {
          if (currCol->IsPrimary())
            break;
          currX += currCol->GetWidth();
        }
        PrefillPropertyArray(mDropRow, currCol);

        // Resolve the style to use for the drop feedback.
        nsCOMPtr<nsIStyleContext> feedbackContext;
        GetPseudoStyleContext(nsCSSAnonBoxes::moztreedropfeedback, getter_AddRefs(feedbackContext));

        const nsStyleVisibility* vis = 
          (const nsStyleVisibility*)feedbackContext->GetStyleData(eStyleStruct_Visibility);
    
        // Paint only if it is visible.
        if (vis->IsVisibleOrCollapsed()) {
          PRInt32 level;
          mView->GetLevel(mDropRow, &level);

          // If our previous or next row has greater level use that for 
          // correct visual indentation.
          if (mDropOrient == nsITreeView::inDropBefore) {
            if (mDropRow > 0) {
              PRInt32 previousLevel;
              mView->GetLevel(mDropRow - 1, &previousLevel);
              if (previousLevel > level)
                level = previousLevel;
            }
          }
          else {
            PRInt32 rowCount;
            mView->GetRowCount(&rowCount);
            if (mDropRow < rowCount - 1) {
              PRInt32 nextLevel;
              mView->GetLevel(mDropRow + 1, &nextLevel);
              if (nextLevel > level)
                level = nextLevel;
            }
          }

          currX += mIndentation * level;

          nsCOMPtr<nsIStyleContext> twistyContext;
          GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty, getter_AddRefs(twistyContext));
          nsRect twistySize = GetImageSize(mDropRow, currCol->GetID().get(), PR_TRUE, twistyContext);
          const nsStyleMargin* twistyMarginData = (const nsStyleMargin*)twistyContext->GetStyleData(eStyleStruct_Margin);
          nsMargin twistyMargin;
          twistyMarginData->GetMargin(twistyMargin);
          twistySize.Inflate(twistyMargin);
          currX += twistySize.width;

          const nsStylePosition* stylePosition = (const nsStylePosition*)
            feedbackContext->GetStyleData(eStyleStruct_Position);

          // Obtain the width for the drop feedback or use default value.
          nscoord width;
          if (stylePosition->mWidth.GetUnit() == eStyleUnit_Coord)
            width = stylePosition->mWidth.GetCoordValue();
          else {
            // Use default width 50px.
            float p2t;
            mPresContext->GetPixelsToTwips(&p2t);
            width = NSIntPixelsToTwips(50, p2t);
          }

          // Obtain the height for the drop feedback or use default value.
          nscoord height;
          if (stylePosition->mHeight.GetUnit() == eStyleUnit_Coord)
            height = stylePosition->mHeight.GetCoordValue();
          else {
            // Use default height 2px.
            float p2t;
            mPresContext->GetPixelsToTwips(&p2t);
            height = NSIntPixelsToTwips(2, p2t);
          }

          // Obtain the margins for the drop feedback and then deflate our rect
          // by that amount.
          nsRect feedbackRect(currX, rowRect.y, width, height);
          const nsStyleMargin* styleMargin = (const nsStyleMargin*)
            feedbackContext->GetStyleData(eStyleStruct_Margin);
          nsMargin margin;
          styleMargin->GetMargin(margin);
          feedbackRect.Deflate(margin);

          // Finally paint the drop feedback.
          PaintBackgroundLayer(feedbackContext, aPresContext, aRenderingContext, feedbackRect, aDirtyRect);
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsTreeBodyFrame::PaintBackgroundLayer(nsIStyleContext*     aStyleContext,
                                      nsIPresContext*      aPresContext, 
                                      nsIRenderingContext& aRenderingContext, 
                                      const nsRect&        aRect,
                                      const nsRect&        aDirtyRect)
{

  const nsStyleBackground* myColor = (const nsStyleBackground*)
      aStyleContext->GetStyleData(eStyleStruct_Background);
  const nsStyleBorder* myBorder = (const nsStyleBorder*)
      aStyleContext->GetStyleData(eStyleStruct_Border);
  const nsStylePadding* myPadding = (const nsStylePadding*)
      aStyleContext->GetStyleData(eStyleStruct_Padding);
  const nsStyleOutline* myOutline = (const nsStyleOutline*)
      aStyleContext->GetStyleData(eStyleStruct_Outline);
  
  nsCSSRendering::PaintBackgroundWithSC(aPresContext, aRenderingContext,
                                        this, aDirtyRect, aRect,
                                        *myColor, *myBorder, *myPadding,
                                        0, 0);

  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, aRect, *myBorder, mStyleContext, 0);

  nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                               aDirtyRect, aRect, *myBorder, *myOutline,
                               aStyleContext, 0);

  return NS_OK;
}

// Scrolling
NS_IMETHODIMP nsTreeBodyFrame::EnsureRowIsVisible(PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  if (mTopRowIndex <= aRow && mTopRowIndex+mPageCount > aRow)
    return NS_OK;

  if (aRow < mTopRowIndex)
    ScrollToRow(aRow);
  else {
    // Bring it just on-screen.
    PRInt32 distance = aRow - (mTopRowIndex+mPageCount)+1;
    ScrollToRow(mTopRowIndex+distance);
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollToRow(PRInt32 aRow)
{
  ScrollInternal(aRow);
  UpdateScrollbar();

#if defined(XP_MAC) || defined(XP_MACOSX)
  // mac can't process the event loop during a drag, so if we're dragging,
  // grab the scroll widget and make it paint synchronously. This is
  // sorta slow (having to paint the entire tree), but it works.
  if ( mDragSession ) {
    nsCOMPtr<nsIWidget> scrollWidget;
    mScrollbar->GetWindow(mPresContext, getter_AddRefs(scrollWidget));
    if ( scrollWidget )
      scrollWidget->Invalidate(PR_TRUE);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollByLines(PRInt32 aNumLines)
{
  if (!mView)
    return NS_OK;

  PRInt32 newIndex = mTopRowIndex + aNumLines;
  if (newIndex < 0)
    newIndex = 0;
  else {
    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    PRInt32 lastPageTopRow = rowCount - mPageCount;
    if (newIndex > lastPageTopRow)
      newIndex = lastPageTopRow;
  }
  ScrollToRow(newIndex);
  
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollByPages(PRInt32 aNumPages)
{
  if (!mView)
    return NS_OK;

  PRInt32 newIndex = mTopRowIndex + aNumPages * mPageCount;
  if (newIndex < 0)
    newIndex = 0;
  else {
    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    PRInt32 lastPageTopRow = rowCount - mPageCount;
    if (newIndex > lastPageTopRow)
      newIndex = lastPageTopRow;
  }
  ScrollToRow(newIndex);
    
  return NS_OK;
}

nsresult
nsTreeBodyFrame::ScrollInternal(PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);

  PRInt32 delta = aRow - mTopRowIndex;

  if (delta > 0) {
    if (mTopRowIndex == (rowCount - mPageCount + 1))
      return NS_OK;
  }
  else {
    if (mTopRowIndex == 0)
      return NS_OK;
  }

  mTopRowIndex += delta;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  // See if we have a background image.  If we do, then we cannot blit.
  const nsStyleBackground* myColor = (const nsStyleBackground*)
      mStyleContext->GetStyleData(eStyleStruct_Background);
  PRBool hasBackground = myColor->mBackgroundImage.Length() > 0;

  PRInt32 absDelta = PR_ABS(delta);
  if (hasBackground || absDelta*mRowHeight >= mRect.height)
    Invalidate();
  else if (mTreeWidget)
    mTreeWidget->Scroll(0, -delta*rowHeightAsPixels, nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aNewIndex > aOldIndex)
    ScrollToRow(mTopRowIndex+1);
  else if (aNewIndex < aOldIndex)
    ScrollToRow(mTopRowIndex-1);
  return NS_OK;
}
  
NS_IMETHODIMP
nsTreeBodyFrame::PositionChanged(PRInt32 aOldIndex, PRInt32& aNewIndex)
{
  if (!EnsureScrollbar())
    return NS_ERROR_UNEXPECTED;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rh = NSToCoordRound((float)mRowHeight*t2p);

  nscoord oldrow = aOldIndex/rh;
  nscoord newrow = aNewIndex/rh;

  if (oldrow != newrow)
    ScrollInternal(newrow);

  // Go exactly where we're supposed to
  // Update the scrollbar.
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  nsAutoString curPos;
  curPos.AppendInt(aNewIndex);
  scrollbarContent->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, curPos, PR_TRUE);

  return NS_OK;
}

// The style cache.
nsresult 
nsTreeBodyFrame::GetPseudoStyleContext(nsIAtom* aPseudoElement, 
                                           nsIStyleContext** aResult)
{
  return mStyleCache.GetStyleContext(this, mPresContext, mContent, mStyleContext, aPseudoElement,
                                     mScratchArray, aResult);
}

// Our comparator for resolving our complex pseudos
NS_IMETHODIMP
nsTreeBodyFrame::PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult)
{
  if (aSelector->mTag == aTag) {
    // Iterate the pseudoclass list.  For each item in the list, see if
    // it is contained in our scratch array.  If we have a miss, then
    // we aren't a match.  If all items in the pseudoclass list are
    // present in the scratch array, then we have a match.
    nsAtomStringList* curr = aSelector->mPseudoClassList;
    while (curr) {
      PRInt32 index;
      mScratchArray->GetIndexOf(curr->mAtom, &index);
      if (index == -1) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
      curr = curr->mNext;
    }
    *aResult = PR_TRUE;
  }
  else 
    *aResult = PR_FALSE;

  return NS_OK;
}

void
nsTreeBodyFrame::InvalidateColumnCache()
{
  mColumnsDirty = PR_TRUE;
}

void
nsTreeBodyFrame::EnsureColumns()
{
  if (!mColumns || mColumnsDirty) {
    delete mColumns;
    mColumnsDirty = PR_FALSE;

    nsCOMPtr<nsIContent> parent;
    GetBaseElement(getter_AddRefs(parent));

    if (!parent)
      return;

    nsCOMPtr<nsIPresShell> shell; 
    mPresContext->GetShell(getter_AddRefs(shell));

    // Note: this is dependent on the anonymous content for select
    // defined in select.xml
    nsCOMPtr<nsIAtom> parentTag;
    parent->GetTag(*getter_AddRefs(parentTag));
    if (parentTag == nsHTMLAtoms::select) {
      // We can avoid crawling the content nodes in this case, since we know
      // that we have a single column, and we know where it's at.

      ChildIterator iter, last;
      ChildIterator::Init(parent, &iter, &last);
      nsCOMPtr<nsIContent> treeCols = *iter;
      nsCOMPtr<nsIContent> column;
      treeCols->ChildAt(0, *getter_AddRefs(column));

      nsIFrame* colFrame = nsnull;
      shell->GetPrimaryFrameFor(column, &colFrame);
      mColumns = new nsTreeColumn(column, colFrame);
      return;
    }

    nsCOMPtr<nsIContent> colsContent;
    nsTreeUtils::GetImmediateChild(parent, nsXULAtoms::treecols, getter_AddRefs(colsContent));
    if (!colsContent)
      return;

    nsIFrame* colsFrame = nsnull;
    shell->GetPrimaryFrameFor(colsContent, &colsFrame);
    if (!colsFrame)
      return;

    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    PRBool normalDirection = (vis->mDirection == NS_STYLE_DIRECTION_LTR);

    nsIBox* colsBox;
    CallQueryInterface(colsFrame, &colsBox);
    nsIBox* colBox = nsnull;
    colsBox->GetChildBox(&colBox);
    nsTreeColumn* currCol = nsnull;
    while (colBox) {
      nsIFrame* frame = nsnull;
      colBox->GetFrame(&frame);
      nsCOMPtr<nsIContent> content;
      frame->GetContent(getter_AddRefs(content));
      nsCOMPtr<nsIAtom> tag;
      content->GetTag(*getter_AddRefs(tag));
      if (tag == nsXULAtoms::treecol) {
        // Create a new column structure.
        nsTreeColumn* col = new nsTreeColumn(content, frame);
        if (normalDirection) {
          if (currCol)
            currCol->SetNext(col);
          else
            mColumns = col;
          currCol = col;
        }
        else {
          col->SetNext(mColumns);
          mColumns = col;
        }
      }

      colBox->GetNextBox(&colBox);
    }
  }
}

nsresult
nsTreeBodyFrame::GetBaseElement(nsIContent** aContent)
{
  nsCOMPtr<nsIContent> parent = mContent;
  nsCOMPtr<nsIAtom> tag;
  nsCOMPtr<nsIContent> temp;

  while (parent && NS_SUCCEEDED(parent->GetTag(*getter_AddRefs(tag)))
         && tag != nsXULAtoms::tree && tag != nsHTMLAtoms::select) {
    temp = parent;
    temp->GetParent(*getter_AddRefs(parent));
  }

  *aContent = parent;
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::ClearStyleAndImageCaches()
{
  mStyleCache.Clear();
  delete mImageCache;
  mImageCache = nsnull;
  mScrollbar = nsnull;
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark - 
#endif

// Tell the view where the drop happened
NS_IMETHODIMP
nsTreeBodyFrame::OnDragDrop (nsIDOMEvent* aEvent)
{
  mView->Drop (mDropRow, mDropOrient);
  return NS_OK;
} // OnDragDrop

// Clear out all our tracking vars.
NS_IMETHODIMP
nsTreeBodyFrame::OnDragExit(nsIDOMEvent* aEvent)
{
  if (mDropAllowed) {
    mDropAllowed = PR_FALSE;
    InvalidatePrimaryCell(mDropRow + (mDropOrient == nsITreeView::inDropAfter ? 1 : 0));
  }
  else
    mDropAllowed = PR_FALSE;
  mDropRow = -1;
  mDropOrient = -1;
  mDragSession = nsnull;
  mScrollLines = 0;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  return NS_OK;
} // OnDragExit

// The mouse is hovering over this tree. If we determine things are different from the
// last time, invalidate primary cell at the old position, query the view to see if the current location is 
// droppable, and then invalidate primary cell at the new location if it is. The mouse may or may
// not have changed position from the last time we were called, so optimize out a lot of
// the extra notifications by checking if anything changed first.
// For drop feedback we use drop, dropBefore and dropAfter property.
NS_IMETHODIMP
nsTreeBodyFrame::OnDragOver(nsIDOMEvent* aEvent)
{
  if (! mView)
    return NS_OK;

  // Save last values, we will need them.
  PRInt32 lastDropRow = mDropRow;
  PRInt16 lastDropOrient = mDropOrient;
  PRInt16 lastScrollLines = mScrollLines;

  // Compute the row mouse is over and the above/below/on state.
  // Below we'll use this to see if anything changed.
  // Also check if we want to auto-scroll.
  ComputeDropPosition(aEvent, &mDropRow, &mDropOrient, &mScrollLines);

  // While we're here, handle tracking of scrolling during a drag.
  if (mScrollLines) {
    if (mDropAllowed) {
      // Invalidate primary cell at old location.
      mDropAllowed = PR_FALSE;
      InvalidatePrimaryCell(lastDropRow + (lastDropOrient == nsITreeView::inDropAfter ? 1 : 0));
    }
#if !defined(XP_MAC) && !defined(XP_MACOSX)
    if (!lastScrollLines) {
      // Entering the scroll region.
      // Set a timer to trigger the tree scrolling.
      if (mTimer) {
        mTimer->Cancel();
        mTimer = nsnull;
      }

      mTimer = do_CreateInstance("@mozilla.org/timer;1");

      nsCOMPtr<nsITimerInternal> ti = do_QueryInterface(mTimer);
      ti->SetIdle(PR_FALSE);

      mTimer->InitWithFuncCallback(ScrollCallback, this, kLazyScrollDelay, 
                                   nsITimer::TYPE_REPEATING_SLACK);

     }
#else
    ScrollByLines(mScrollLines);
#endif
    // Bail out to prevent spring loaded timer and feedback line settings.
    return NS_OK;
  }

  // If changed from last time, invalidate primary cell at the old location and if allowed, 
  // invalidate primary cell at the new location. If nothing changed, just bail.
  if (mDropRow != lastDropRow || mDropOrient != lastDropOrient) {
    // Invalidate row at the old location.
    if (mDropAllowed) {
      mDropAllowed = PR_FALSE;
      InvalidatePrimaryCell(lastDropRow + (lastDropOrient == nsITreeView::inDropAfter ? 1 : 0));
    }

    if (mTimer) {
      // Timer is active but for a different row than the current one, kill it.
      mTimer->Cancel();
      mTimer = nsnull;
    }

    if (mDropRow >= 0) {
      if (!mTimer && mDropOrient == nsITreeView::inDropOn) {
        // Either there wasn't a timer running or it was just killed above.
        // If over a folder, start up a timer to open the folder.
        PRBool isContainer = PR_FALSE;
        mView->IsContainer(mDropRow, &isContainer);
        if (isContainer) {
          PRBool isOpen = PR_FALSE;
          mView->IsContainerOpen(mDropRow, &isOpen);
          if (!isOpen) {
            // This node isn't expanded, set a timer to expand it.
            mTimer = do_CreateInstance("@mozilla.org/timer;1");
            nsCOMPtr<nsITimerInternal> ti = do_QueryInterface(mTimer);
            ti->SetIdle(PR_FALSE);
            mTimer->InitWithFuncCallback(OpenCallback, this, kOpenDelay, 
                                         nsITimer::TYPE_ONE_SHOT);
          }
        }
      }

      PRBool canDropAtNewLocation = PR_FALSE;
      if (mDropOrient == nsITreeView::inDropOn)
        mView->CanDropOn(mDropRow, &canDropAtNewLocation);
      else
        mView->CanDropBeforeAfter (mDropRow, mDropOrient == nsITreeView::inDropBefore ? PR_TRUE : PR_FALSE, &canDropAtNewLocation);
      
      if (canDropAtNewLocation) {
        // Invalidate row at the new location.
        mDropAllowed = canDropAtNewLocation;
        InvalidatePrimaryCell(mDropRow + (mDropOrient == nsITreeView::inDropAfter ? 1 : 0));
      }
    }
  }
  
  // alert the drag session we accept the drop. We have to do this every time
  // since the |canDrop| attribute is reset before we're called.
  if (mDropAllowed && mDragSession)
    mDragSession->SetCanDrop(PR_TRUE);

  // Prevent default handler to fire.
  aEvent->PreventDefault();

  return NS_OK;
} // OnDragOver

PRBool
nsTreeBodyFrame::CanAutoScroll(PRInt32 aRowIndex)
{
  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);

  // Check first for partially visible last row.
  if (aRowIndex == rowCount - 1) {
    nscoord y = mInnerBox.y + (aRowIndex - mTopRowIndex) * mRowHeight;
    if (y < mInnerBox.height && y + mRowHeight > mInnerBox.height)
      return PR_TRUE;
  }

  if (aRowIndex > 0 && aRowIndex < rowCount - 1)
    return PR_TRUE;

  return PR_FALSE;
}

// Given a dom event, figure out which row in the tree the mouse is over,
// if we should drop before/after/on that row or we should auto-scroll.
// Doesn't query the content about if the drag is allowable, that's done elsewhere.
//
// For containers, we break up the vertical space of the row as follows: if in
// the topmost 25%, the drop is _before_ the row the mouse is over; if in the
// last 25%, _after_; in the middle 50%, we consider it a drop _on_ the container.
//
// For non-containers, if the mouse is in the top 50% of the row, the drop is
// _before_ and the bottom 50% _after_
void 
nsTreeBodyFrame::ComputeDropPosition(nsIDOMEvent* aEvent, PRInt32* aRow, PRInt16* aOrient,
                                     PRInt16* aScrollLines)
{
  *aRow = -1;
  *aOrient = -1;
  *aScrollLines = 0;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent (do_QueryInterface(aEvent));
  if (mouseEvent) {
    PRInt32 x = 0;
    PRInt32 y = 0;
    mouseEvent->GetClientX(&x);
    mouseEvent->GetClientY(&y);
    
    PRInt32 xTwips;
    PRInt32 yTwips;
    AdjustEventCoordsToBoxCoordSpace (x, y, &xTwips, &yTwips);

    GetRowAt(x, y, aRow);
    if (*aRow >=0) {
      // Compute the top/bottom of the row in question.
      PRInt32 yOffset = yTwips - mRowHeight * (*aRow - mTopRowIndex);
   
      PRBool isContainer = PR_FALSE;
      mView->IsContainer (*aRow, &isContainer);
      if (isContainer) {
        // for a container, use a 25%/50%/25% breakdown
        if (yOffset < mRowHeight / 4)
          *aOrient = nsITreeView::inDropBefore;
        else if (yOffset > mRowHeight - (mRowHeight / 4))
          *aOrient = nsITreeView::inDropAfter;
        else
          *aOrient = nsITreeView::inDropOn;
      }
      else {
        // for a non-container use a 50%/50% breakdown
        if (yOffset < mRowHeight / 2)
          *aOrient = nsITreeView::inDropBefore;
        else
          *aOrient = nsITreeView::inDropAfter;
      }
    }

    if (CanAutoScroll(*aRow)) {
      // Determine if we're w/in a margin of the top/bottom of the tree during a drag.
      // This will ultimately cause us to scroll, but that's done elsewhere.
      PRInt32 height = (2 * mRowHeight) / 3;
      if (yTwips < height) {
        // scroll up
        *aScrollLines = -1;
      }
      else if (yTwips > mRect.height - height) {
        // scroll down
        *aScrollLines = 1;
      }
    }
  }
} // ComputeDropPosition

// Cache several things we'll need throughout the course of our work. These
// will all get released on a drag exit
NS_IMETHODIMP
nsTreeBodyFrame::OnDragEnter(nsIDOMEvent* aEvent)
{
  // cache the drag session
  nsresult rv;
  nsCOMPtr<nsIDragService> dragService = 
           do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(mDragSession));
  NS_ASSERTION(mDragSession, "can't get drag session");

  return NS_OK;
} // OnDragEnter

void
nsTreeBodyFrame::OpenCallback(nsITimer *aTimer, void *aClosure)
{
  nsTreeBodyFrame* self = NS_STATIC_CAST(nsTreeBodyFrame*, aClosure);
  if (self) {
    aTimer->Cancel();
    self->mTimer = nsnull;

    if (self->mDropRow >= 0)
      self->mView->ToggleOpenState(self->mDropRow);
  }
}

void
nsTreeBodyFrame::ScrollCallback(nsITimer *aTimer, void *aClosure)
{
  nsTreeBodyFrame* self = NS_STATIC_CAST(nsTreeBodyFrame*, aClosure);
  if (self) {
    // Don't scroll if we are already at the top or bottom of the view.
    if (self->mView && self->CanAutoScroll(self->mDropRow)) {
      self->ScrollByLines(self->mScrollLines);
      PRUint32 delay = 0;
      aTimer->GetDelay(&delay);
      if (delay != kScrollDelay)
        aTimer->SetDelay(kScrollDelay);
    }
    else {
      aTimer->Cancel();
      self->mTimer = nsnull;
    }
  }
}

#ifdef XP_MAC
#pragma mark - 
#endif


// ==============================================================================
// The ImageListener implementation
// ==============================================================================

NS_IMPL_ISUPPORTS3(nsTreeImageListener, imgIDecoderObserver, imgIContainerObserver, nsITreeImageListener)

nsTreeImageListener::nsTreeImageListener(nsITreeBoxObject* aTree, const PRUnichar* aColID)
{
  NS_INIT_ISUPPORTS();
  mTree = aTree;
  mColID = aColID;
  mMin = -1; // min should start out "undefined"
  mMax = 0;
}

nsTreeImageListener::~nsTreeImageListener()
{
}

NS_IMETHODIMP nsTreeImageListener::OnStartDecode(imgIRequest *aRequest, nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnStartContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnStartFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnDataAvailable(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnStopFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnStopContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnStopDecode(imgIRequest *aRequest, nsISupports *aContext, nsresult status, const PRUnichar *statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::FrameChanged(imgIContainer *aContainer, nsISupports *aContext, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  Invalidate();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeImageListener::AddRow(int aIndex)
{
  if (mMin == -1)
    mMin = mMax = aIndex;
  else if (aIndex < mMin)
    mMin = aIndex;
  else if (aIndex > mMax)
    mMax = aIndex;

  return NS_OK;
}

NS_IMETHODIMP 
nsTreeImageListener::Invalidate()
{
  // Loop from min to max, invalidating each cell that was listening for this image.
  for (PRInt32 i = mMin; i <= mMax; i++) {
    mTree->InvalidateCell(i, mColID.get());
  }

  return NS_OK;
}
