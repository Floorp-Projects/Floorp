/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributors:
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Simon Fraser  <sfraser@netscape.com>
 *   Akkana Peck  <akkana@netscape.com>
 */

#include "nsWebBrowserFind.h"

// Only need this for NS_FIND_CONTRACTID,
// else we could use nsIDOMRange.h and nsIFind.h.
#include "nsFind.h"

#include "nsIComponentManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIEnumerator.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIFocusController.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsReadableUtils.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIContent.h"
#include "nsContentCID.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsITimelineService.h"

#if DEBUG
#include "nsIWebNavigation.h"
#include "nsXPIDLString.h"
#endif

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

//*****************************************************************************
// nsWebBrowserFind
//*****************************************************************************

nsWebBrowserFind::nsWebBrowserFind() :
    mFindBackwards(PR_FALSE),
    mWrapFind(PR_FALSE),
    mEntireWord(PR_FALSE),
    mMatchCase(PR_FALSE),
    mSearchSubFrames(PR_TRUE),
    mSearchParentFrames(PR_TRUE)
{
}

nsWebBrowserFind::~nsWebBrowserFind()
{
}

NS_IMPL_ISUPPORTS2(nsWebBrowserFind, nsIWebBrowserFind, nsIWebBrowserFindInFrames);


/* boolean findNext (); */
NS_IMETHODIMP nsWebBrowserFind::FindNext(PRBool *outDidFind)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(outDidFind);
    *outDidFind = PR_FALSE;

    NS_ENSURE_TRUE(CanFindNext(), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mCurrentSearchFrame);
    NS_ENSURE_TRUE(searchFrame, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDOMWindow> rootFrame = do_QueryReferent(mRootSearchFrame);
    NS_ENSURE_TRUE(searchFrame, NS_ERROR_NOT_INITIALIZED);
    
    // first, if there's a "cmd_findagain" observer around, check to see if it
    // wants to perform the find again command . If it performs the find again
    // it will return true, in which case we exit ::FindNext() early.
    // Otherwise, nsWebBrowserFind needs to perform the find again command itself
    // this is used by nsTypeAheadFind, which controls find again when it was
    // the last executed find in the current window.
    nsCOMPtr<nsIObserverService> observerSvc =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerSvc) {
        nsCOMPtr<nsISupportsInterfacePointer> windowSupportsData = 
          do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsISupports> searchWindowSupports =
          do_QueryInterface(rootFrame);
        windowSupportsData->SetData(searchWindowSupports);
        NS_NAMED_LITERAL_STRING(dnStr, "down");
        NS_NAMED_LITERAL_STRING(upStr, "up");
        observerSvc->NotifyObservers(windowSupportsData, 
                                     "nsWebBrowserFind_FindAgain", 
                                     mFindBackwards? upStr.get(): dnStr.get());
        windowSupportsData->GetData(getter_AddRefs(searchWindowSupports));
        // findnext performed if search window data cleared out
        *outDidFind = searchWindowSupports == nsnull;
        if (*outDidFind)
            return NS_OK;
    }

    // next, look in the current frame. If found, return.
    rv = SearchInFrame(searchFrame, PR_FALSE, outDidFind);
    if (NS_FAILED(rv)) return rv;
    if (*outDidFind)
        return OnFind(searchFrame);     // we are done

    // if we are not searching other frames, return
    if (!mSearchSubFrames && !mSearchParentFrames)
        return NS_OK;

    nsCOMPtr<nsIDocShell>  rootDocShell;
    rv = GetDocShellFromWindow(rootFrame, getter_AddRefs(rootDocShell));
    if (NS_FAILED(rv)) return rv;
    
    PRInt32 enumDirection;
    if (mFindBackwards)
        enumDirection = nsIDocShell::ENUMERATE_BACKWARDS;
    else
        enumDirection = nsIDocShell::ENUMERATE_FORWARDS;
        
    nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
    rv = rootDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
            enumDirection, getter_AddRefs(docShellEnumerator));    
    if (NS_FAILED(rv)) return rv;
        
    // remember where we started
    nsCOMPtr<nsIDocShell>  startingShell;
    rv = GetDocShellFromWindow(searchFrame, getter_AddRefs(startingShell));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDocShellTreeItem> startingItem = do_QueryInterface(startingShell, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIDocShellTreeItem> curItem;

    // XXX We should avoid searching in frameset documents here.
    // We also need to honour mSearchSubFrames and mSearchParentFrames.
    PRBool hasMore, doFind = PR_FALSE;
    while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> curSupports;
        rv = docShellEnumerator->GetNext(getter_AddRefs(curSupports));
        if (NS_FAILED(rv)) break;
        curItem = do_QueryInterface(curSupports, &rv);
        if (NS_FAILED(rv)) break;

        if (doFind)
        {
            searchFrame = do_GetInterface(curItem, &rv);
            if (NS_FAILED(rv)) break;

            OnStartSearchFrame(searchFrame);

            rv = SearchInFrame(searchFrame, PR_FALSE, outDidFind);
            if (NS_FAILED(rv)) return rv;
            if (*outDidFind)
                return OnFind(searchFrame);     // we are done

            OnEndSearchFrame(searchFrame);
        }

        if (curItem.get() == startingItem.get())
            doFind = PR_TRUE;       // start looking in frames after this one
    };

    if (!mWrapFind)
    {
        // remember where we left off
        SetCurrentSearchFrame(searchFrame);
        return NS_OK;
    }

    // From here on, we're wrapping, first through the other frames,
    // then finally from the beginning of the starting frame back to
    // the starting point.

    // because nsISimpleEnumerator is totally lame and isn't resettable, I
    // have to make a new one
    docShellEnumerator = nsnull;
    rv = rootDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
            enumDirection, getter_AddRefs(docShellEnumerator));    
    if (NS_FAILED(rv)) return rv;
    
    while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> curSupports;
        rv = docShellEnumerator->GetNext(getter_AddRefs(curSupports));
        if (NS_FAILED(rv)) break;
        curItem = do_QueryInterface(curSupports, &rv);
        if (NS_FAILED(rv)) break;

        if (curItem.get() == startingItem.get())
        {
            rv = SearchInFrame(searchFrame, PR_TRUE, outDidFind);
            if (NS_FAILED(rv)) return rv;
            if (*outDidFind)
                return OnFind(searchFrame);        // we are done
            break;
        }

        searchFrame = do_GetInterface(curItem, &rv);
        if (NS_FAILED(rv)) break;

        OnStartSearchFrame(searchFrame);

        rv = SearchInFrame(searchFrame, PR_FALSE, outDidFind);
        if (NS_FAILED(rv)) return rv;
        if (*outDidFind)
            return OnFind(searchFrame);        // we are done
        
        OnEndSearchFrame(searchFrame);
    }

    // remember where we left off
    SetCurrentSearchFrame(searchFrame);
    
    NS_ASSERTION(NS_SUCCEEDED(rv), "Something failed");
    return rv;
}


/* attribute wstring searchString; */
NS_IMETHODIMP nsWebBrowserFind::GetSearchString(PRUnichar * *aSearchString)
{
    NS_ENSURE_ARG_POINTER(aSearchString);
    *aSearchString = ToNewUnicode(mSearchString);
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserFind::SetSearchString(const PRUnichar * aSearchString)
{
    mSearchString.Assign(aSearchString);
    return NS_OK;
}

/* attribute boolean findBackwards; */
NS_IMETHODIMP nsWebBrowserFind::GetFindBackwards(PRBool *aFindBackwards)
{
    NS_ENSURE_ARG_POINTER(aFindBackwards);
    *aFindBackwards = mFindBackwards;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserFind::SetFindBackwards(PRBool aFindBackwards)
{
    mFindBackwards = aFindBackwards;
    return NS_OK;
}

/* attribute boolean wrapFind; */
NS_IMETHODIMP nsWebBrowserFind::GetWrapFind(PRBool *aWrapFind)
{
    NS_ENSURE_ARG_POINTER(aWrapFind);
    *aWrapFind = mWrapFind;
    return NS_OK;
}
NS_IMETHODIMP nsWebBrowserFind::SetWrapFind(PRBool aWrapFind)
{
    mWrapFind = aWrapFind;
    return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP nsWebBrowserFind::GetEntireWord(PRBool *aEntireWord)
{
    NS_ENSURE_ARG_POINTER(aEntireWord);
    *aEntireWord = mEntireWord;
    return NS_OK;
}
NS_IMETHODIMP nsWebBrowserFind::SetEntireWord(PRBool aEntireWord)
{
    mEntireWord = aEntireWord;
    return NS_OK;
}

/* attribute boolean matchCase; */
NS_IMETHODIMP nsWebBrowserFind::GetMatchCase(PRBool *aMatchCase)
{
    NS_ENSURE_ARG_POINTER(aMatchCase);
    *aMatchCase = mMatchCase;
    return NS_OK;
}
NS_IMETHODIMP nsWebBrowserFind::SetMatchCase(PRBool aMatchCase)
{
    mMatchCase = aMatchCase;
    return NS_OK;
}

void nsWebBrowserFind::SetSelectionAndScroll(nsIDOMRange* aRange,
                                             nsISelectionController *aSelCon)
{
  nsCOMPtr<nsISelection> selection;
  if (!aSelCon) return;

  aSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                        getter_AddRefs(selection));
  if (!selection) return;
  selection->RemoveAllRanges();
  selection->AddRange(aRange);

  // Scroll if necessary to make the selection visible:
  aSelCon->ScrollSelectionIntoView
    (nsISelectionController::SELECTION_NORMAL,
     nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
}

// Adapted from nsTextServicesDocument::GetDocumentContentRootNode
nsresult nsWebBrowserFind::GetRootNode(nsIDOMDocument* aDomDoc,
                                       nsIDOMNode **aNode)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aNode);
  *aNode = 0;

  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(aDomDoc);
  if (htmlDoc)
  {
    // For HTML documents, the content root node is the body.
    nsCOMPtr<nsIDOMHTMLElement> bodyElement;
    rv = htmlDoc->GetBody(getter_AddRefs(bodyElement));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(bodyElement);
    return bodyElement->QueryInterface(NS_GET_IID(nsIDOMNode),
                                       (void **)aNode);
  }

  // For non-HTML documents, the content root node will be the doc element.
  nsCOMPtr<nsIDOMElement> docElement;
  rv = aDomDoc->GetDocumentElement(getter_AddRefs(docElement));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_POINTER(docElement);
  return docElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aNode);
}

nsresult nsWebBrowserFind::SetRangeAroundDocument(nsIDOMRange* aSearchRange,
                                                  nsIDOMRange* aStartPt,
                                                  nsIDOMRange* aEndPt,
                                                  nsIDOMDocument* aDoc)
{
    nsCOMPtr<nsIDOMNode> bodyNode;
    nsresult rv = GetRootNode(aDoc, getter_AddRefs(bodyNode));
    nsCOMPtr<nsIContent> bodyContent (do_QueryInterface(bodyNode));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(bodyContent);

    PRInt32 childCount;
    rv = bodyContent->ChildCount(childCount);
    NS_ENSURE_SUCCESS(rv, rv);

    aSearchRange->SetStart(bodyNode, 0);
    aSearchRange->SetEnd(bodyNode, childCount);

    if (mFindBackwards)
    {
        aStartPt->SetStart(bodyNode, childCount);
        aStartPt->SetEnd(bodyNode, childCount);
        aEndPt->SetStart(bodyNode, 0);
        aEndPt->SetEnd(bodyNode, 0);
    }
    else
    {
        aStartPt->SetStart(bodyNode, 0);
        aStartPt->SetEnd(bodyNode, 0);
        aEndPt->SetStart(bodyNode, childCount);
        aEndPt->SetEnd(bodyNode, childCount);
    }

    return NS_OK;
}

// Set the range to go from the end of the current selection to the
// end of the document (forward), or beginning to beginning (reverse).
// or around the whole document if there's no selection.
nsresult
nsWebBrowserFind::GetSearchLimits(nsIDOMRange* aSearchRange,
                                  nsIDOMRange* aStartPt,
                                  nsIDOMRange* aEndPt,
                                  nsIDOMDocument* aDoc,
                                  nsISelectionController* aSelCon,
                                  PRBool aWrap)
{
    NS_ENSURE_ARG_POINTER(aSelCon);
    nsCOMPtr<nsISelection> selection;
    aSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                          getter_AddRefs(selection));
    if (!selection) return NS_ERROR_BASE;

    // There is a selection.
    PRInt32 count = -1;
    nsresult rv = selection->GetRangeCount(&count);
    if (count < 1)
        return SetRangeAroundDocument(aSearchRange, aStartPt, aEndPt, aDoc);

    // Need bodyNode, for the start/end of the document
    nsCOMPtr<nsIDOMNode> bodyNode;
    rv = GetRootNode(aDoc, getter_AddRefs(bodyNode));
    nsCOMPtr<nsIContent> bodyContent (do_QueryInterface(bodyNode));
    NS_ENSURE_ARG_POINTER(bodyContent);

    PRInt32 childCount;
    rv = bodyContent->ChildCount(childCount);
    NS_ENSURE_SUCCESS(rv, rv);

    // There are four possible range endpoints we might use:
    // DocumentStart, SelectionStart, SelectionEnd, DocumentEnd.

    nsCOMPtr<nsIDOMRange> range;
    nsCOMPtr<nsIDOMNode> node;
    PRInt32 offset;

    // Forward, not wrapping: SelEnd to DocEnd
    if (!mFindBackwards && !aWrap)
    {
        // This isn't quite right, since the selection's ranges aren't
        // necessarily in order; but they usually will be.
        selection->GetRangeAt(count-1, getter_AddRefs(range));
        if (!range) return NS_ERROR_UNEXPECTED;
        range->GetEndContainer(getter_AddRefs(node));
        if (!node) return NS_ERROR_UNEXPECTED;
        range->GetEndOffset(&offset);

        aSearchRange->SetStart(node, offset);
        aSearchRange->SetEnd(bodyNode, childCount);
        aStartPt->SetStart(node, offset);
        aStartPt->SetEnd(node, offset);
        aEndPt->SetStart(bodyNode, childCount);
        aEndPt->SetEnd(bodyNode, childCount);
    }
    // Backward, not wrapping: DocStart to SelStart
    else if (mFindBackwards && !aWrap)
    {
        selection->GetRangeAt(0, getter_AddRefs(range));
        if (!range) return NS_ERROR_UNEXPECTED;
        range->GetStartContainer(getter_AddRefs(node));
        if (!node) return NS_ERROR_UNEXPECTED;
        range->GetStartOffset(&offset);

        aSearchRange->SetStart(bodyNode, 0);
        aSearchRange->SetEnd(bodyNode, childCount);
        aStartPt->SetStart(node, offset);
        aStartPt->SetEnd(node, offset);
        aEndPt->SetStart(bodyNode, 0);
        aEndPt->SetEnd(bodyNode, 0);
    }
    // Forward, wrapping: DocStart to SelEnd
    else if (!mFindBackwards && aWrap)
    {
        selection->GetRangeAt(count-1, getter_AddRefs(range));
        if (!range) return NS_ERROR_UNEXPECTED;
        range->GetEndContainer(getter_AddRefs(node));
        if (!node) return NS_ERROR_UNEXPECTED;
        range->GetEndOffset(&offset);

        aSearchRange->SetStart(bodyNode, 0);
        aSearchRange->SetEnd(bodyNode, childCount);
        aStartPt->SetStart(bodyNode, 0);
        aStartPt->SetEnd(bodyNode, 0);
        aEndPt->SetStart(node, offset);
        aEndPt->SetEnd(node, offset);
    }
    // Backward, wrapping: SelStart to DocEnd
    else if (mFindBackwards && aWrap)
    {
        selection->GetRangeAt(0, getter_AddRefs(range));
        if (!range) return NS_ERROR_UNEXPECTED;
        range->GetStartContainer(getter_AddRefs(node));
        if (!node) return NS_ERROR_UNEXPECTED;
        range->GetStartOffset(&offset);

        aSearchRange->SetStart(bodyNode, 0);
        aSearchRange->SetEnd(bodyNode, childCount);
        aStartPt->SetStart(bodyNode, childCount);
        aStartPt->SetEnd(bodyNode, childCount);
        aEndPt->SetStart(node, offset);
        aEndPt->SetEnd(node, offset);
    }
    return NS_OK;
}

/* attribute boolean searchFrames; */
NS_IMETHODIMP nsWebBrowserFind::GetSearchFrames(PRBool *aSearchFrames)
{
    NS_ENSURE_ARG_POINTER(aSearchFrames);
    // this only returns true if we are searching both sub and parent
    // frames. There is ambiguity if the caller has previously set
    // one, but not both of these.
    *aSearchFrames = mSearchSubFrames && mSearchParentFrames;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserFind::SetSearchFrames(PRBool aSearchFrames)
{
    mSearchSubFrames = aSearchFrames;
    mSearchParentFrames = aSearchFrames;
    return NS_OK;
}

/* attribute nsIDOMWindow currentSearchFrame; */
NS_IMETHODIMP nsWebBrowserFind::GetCurrentSearchFrame(nsIDOMWindow * *aCurrentSearchFrame)
{
    NS_ENSURE_ARG_POINTER(aCurrentSearchFrame);
    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mCurrentSearchFrame);
    NS_IF_ADDREF(*aCurrentSearchFrame = searchFrame);
    return (*aCurrentSearchFrame) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP nsWebBrowserFind::SetCurrentSearchFrame(nsIDOMWindow * aCurrentSearchFrame)
{
    // is it ever valid to set this to null?
    NS_ENSURE_ARG(aCurrentSearchFrame);
    mCurrentSearchFrame = getter_AddRefs(NS_GetWeakReference(aCurrentSearchFrame));
    return NS_OK;
}

/* attribute nsIDOMWindow rootSearchFrame; */
NS_IMETHODIMP nsWebBrowserFind::GetRootSearchFrame(nsIDOMWindow * *aRootSearchFrame)
{
    NS_ENSURE_ARG_POINTER(aRootSearchFrame);
    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mRootSearchFrame);
    NS_IF_ADDREF(*aRootSearchFrame = searchFrame);
    return (*aRootSearchFrame) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP nsWebBrowserFind::SetRootSearchFrame(nsIDOMWindow * aRootSearchFrame)
{
    // is it ever valid to set this to null?
    NS_ENSURE_ARG(aRootSearchFrame);
    mRootSearchFrame = getter_AddRefs(NS_GetWeakReference(aRootSearchFrame));
    return NS_OK;
}

/* attribute boolean searchSubframes; */
NS_IMETHODIMP nsWebBrowserFind::GetSearchSubframes(PRBool *aSearchSubframes)
{
    NS_ENSURE_ARG_POINTER(aSearchSubframes);
    *aSearchSubframes = mSearchSubFrames;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserFind::SetSearchSubframes(PRBool aSearchSubframes)
{
    mSearchSubFrames = aSearchSubframes;
    return NS_OK;
}

/* attribute boolean searchParentFrames; */
NS_IMETHODIMP nsWebBrowserFind::GetSearchParentFrames(PRBool *aSearchParentFrames)
{
    NS_ENSURE_ARG_POINTER(aSearchParentFrames);
    *aSearchParentFrames = mSearchParentFrames;
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserFind::SetSearchParentFrames(PRBool aSearchParentFrames)
{
    mSearchParentFrames = aSearchParentFrames;
    return NS_OK;
}

void nsWebBrowserFind::MoveFocusToCaret(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
  if (!docShell)
    return;
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell) 
    return;

  nsCOMPtr<nsIPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext) 
    return;

  nsCOMPtr<nsIEventStateManager> esm;
  presContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    PRBool isSelectionWithFocus;
    esm->MoveFocusToCaret(PR_TRUE, &isSelectionWithFocus);
  }
}

/*
    This method handles finding in a single window (aka frame).

*/
nsresult nsWebBrowserFind::SearchInFrame(nsIDOMWindow* aWindow,
                                         PRBool aWrapping,
                                         PRBool* aDidFind)
{
    NS_ENSURE_ARG(aWindow);
    NS_ENSURE_ARG_POINTER(aDidFind);

    *aDidFind = PR_FALSE;

    nsresult rv = NS_OK;
    nsCOMPtr<nsIPresShell> presShell;

    nsCOMPtr<nsIDOMDocument> domDoc;    
    rv = aWindow->GetDocument(getter_AddRefs(domDoc));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(domDoc);

    // if this is a different frame to last time, throw away the mTSFind
    // and make a new one. The nsIFindAndReplace is *not* stateless;
    // it remembers the last search offset etc.
    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mCurrentSearchFrame);    
    // Get the selection controller -- we'd better do this every time,
    // since the doc might have changed since the last time.
    nsCOMPtr<nsIDocShell> docShell;
    rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(docShell);
    docShell->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_ARG_POINTER(presShell);

    if (!mFind)
        mFind = do_CreateInstance(NS_FIND_CONTRACTID, &rv);

#if DEBUG_smfr
    {
        nsCOMPtr<nsIDocShell>  searchShell;
        GetDocShellFromWindow(searchFrame, getter_AddRefs(searchShell));
        nsCOMPtr<nsIWebNavigation> webnav = do_QueryInterface(searchShell);
        if (webnav)
        {
            nsCOMPtr<nsIURI> curURI;
            webnav->GetCurrentURI(getter_AddRefs(curURI));
            nsXPIDLCString   uriSpec;
            if (curURI)
                curURI->GetSpec(getter_Copies(uriSpec));
            
            printf("Searching frame %s\n", (const char*)uriSpec);
        }
    }
#endif

    (void) mFind->SetCaseSensitive(mMatchCase);
    (void) mFind->SetFindBackwards(mFindBackwards);

    // XXX Make and set a line breaker here, once that's implemented.
    (void) mFind->SetWordBreaker(0);

    nsCOMPtr<nsISelectionController> selCon (do_QueryInterface(presShell));
    NS_ENSURE_ARG_POINTER(selCon);

    nsCOMPtr<nsIDOMRange> searchRange (do_CreateInstance(kRangeCID));
    NS_ENSURE_ARG_POINTER(searchRange);
    nsCOMPtr<nsIDOMRange> startPt (do_CreateInstance(kRangeCID));
    NS_ENSURE_ARG_POINTER(startPt);
    nsCOMPtr<nsIDOMRange> endPt (do_CreateInstance(kRangeCID));
    NS_ENSURE_ARG_POINTER(endPt);

    nsCOMPtr<nsIDOMRange> foundRange;

    // If !aWrapping, search from selection to end
    if (!aWrapping)
        rv = GetSearchLimits(searchRange, startPt, endPt, domDoc, selCon,
                             PR_FALSE);

    // If aWrapping, search the part of the starting frame
    // up to the point where we left off.
    else
        rv = GetSearchLimits(searchRange, startPt, endPt, domDoc, selCon,
                             PR_TRUE);

    NS_ENSURE_SUCCESS(rv, rv);

    rv =  mFind->Find(mSearchString.get(), searchRange, startPt, endPt,
                      getter_AddRefs(foundRange));

    if (NS_SUCCEEDED(rv) && foundRange)
    {
        *aDidFind = PR_TRUE;
        SetSelectionAndScroll(foundRange, selCon);
    }

    if (*aDidFind) 
      MoveFocusToCaret(aWindow);

    return rv;
}


// called when we start searching a frame that is not the initial
// focussed frame. Prepare the frame to be searched.
// we clear the selection, so that the search starts from the top
// of the frame.
nsresult nsWebBrowserFind::OnStartSearchFrame(nsIDOMWindow *aWindow)
{
    return ClearFrameSelection(aWindow);
}

// called when we are done searching a frame and didn't find anything,
// and about about to start searching the next frame.
nsresult nsWebBrowserFind::OnEndSearchFrame(nsIDOMWindow *aWindow)
{
    return NS_OK;
}

nsresult nsWebBrowserFind::ClearFrameSelection(nsIDOMWindow *aWindow)
{
    NS_ENSURE_ARG(aWindow);
    nsCOMPtr<nsISelection> selection;
    aWindow->GetSelection(getter_AddRefs(selection));
    if (selection)
        selection->RemoveAllRanges();
    
    return NS_OK;
}

nsresult nsWebBrowserFind::OnFind(nsIDOMWindow *aFoundWindow)
{
    SetCurrentSearchFrame(aFoundWindow);

    // We don't want a selection to appear in two frames simultaneously
    nsCOMPtr<nsIDOMWindow> lastFocusedWindow = do_QueryReferent(mLastFocusedWindow);
    if (lastFocusedWindow && lastFocusedWindow != aFoundWindow)
        ClearFrameSelection(lastFocusedWindow);

    // focus the frame we found in
    nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(aFoundWindow);
    nsCOMPtr<nsIFocusController> focusController;
    if (ourWindow)
        ourWindow->GetRootFocusController(getter_AddRefs(focusController));
    if (focusController)
    {
        nsCOMPtr<nsIDOMWindowInternal> windowInt = do_QueryInterface(aFoundWindow);
        focusController->SetFocusedWindow(windowInt);
        mLastFocusedWindow = getter_AddRefs(NS_GetWeakReference(aFoundWindow));
    }

    return NS_OK;
}

/*---------------------------------------------------------------------------

  GetDocShellFromWindow

  Utility method. This will always return an error if no docShell
  is returned. Oh why isn't there a better way to do this?
----------------------------------------------------------------------------*/
nsresult
nsWebBrowserFind::GetDocShellFromWindow(nsIDOMWindow *inWindow, nsIDocShell** outDocShell)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptGO(do_QueryInterface(inWindow));
  if (!scriptGO) return NS_ERROR_FAILURE;

  nsresult rv = scriptGO->GetDocShell(outDocShell);
  if (NS_FAILED(rv)) return rv;
  if (!*outDocShell) return NS_ERROR_FAILURE;
  return NS_OK;
}

