/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Simon Fraser  <sfraser@netscape.com>
 *   Akkana Peck  <akkana@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsWebBrowserFind.h"

// Only need this for NS_FIND_CONTRACTID,
// else we could use nsIDOMRange.h and nsIFind.h.
#include "nsFind.h"

#include "nsIComponentManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIEnumerator.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIFocusController.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
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

#ifdef XP_MACOSX
#include "nsAutoPtr.h"
#include <Scrap.h>
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

NS_IMPL_ISUPPORTS2(nsWebBrowserFind, nsIWebBrowserFind, nsIWebBrowserFindInFrames)


/* boolean findNext (); */
NS_IMETHODIMP nsWebBrowserFind::FindNext(PRBool *outDidFind)
{
    NS_ENSURE_ARG_POINTER(outDidFind);
    *outDidFind = PR_FALSE;

    NS_ENSURE_TRUE(CanFindNext(), NS_ERROR_NOT_INITIALIZED);

    nsresult rv = NS_OK;
    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mCurrentSearchFrame);
    NS_ENSURE_TRUE(searchFrame, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDOMWindow> rootFrame = do_QueryReferent(mRootSearchFrame);
    NS_ENSURE_TRUE(rootFrame, NS_ERROR_NOT_INITIALIZED);
    
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

    nsIDocShell *rootDocShell = GetDocShellFromWindow(rootFrame);
    if (!rootDocShell) return NS_ERROR_FAILURE;
    
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
    nsCOMPtr<nsIDocShellTreeItem> startingItem =
        do_QueryInterface(GetDocShellFromWindow(searchFrame), &rv);
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
#ifdef XP_MACOSX
    OSStatus err;
    ScrapRef scrap;
    err = ::GetScrapByName(kScrapFindScrap, kScrapGetNamedScrap, &scrap);
    if (err == noErr) {
        Size byteCount;
        err = ::GetScrapFlavorSize(scrap, kScrapFlavorTypeUnicode, &byteCount);
        if (err == noErr) {
            NS_ASSERTION(byteCount%2 == 0, "byteCount not a multiple of 2");
            nsAutoArrayPtr<PRUnichar> buffer(new PRUnichar[byteCount/2 + 1]);
            NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);
            err = ::GetScrapFlavorData(scrap, kScrapFlavorTypeUnicode, &byteCount, buffer.get());
            if (err == noErr) {
                buffer[byteCount/2] = PRUnichar('\0');
                mSearchString.Assign(buffer);
            }
        }
    }    
#endif
    *aSearchString = ToNewUnicode(mSearchString);
    return NS_OK;
}

NS_IMETHODIMP nsWebBrowserFind::SetSearchString(const PRUnichar * aSearchString)
{
    mSearchString.Assign(aSearchString);
#ifdef XP_MACOSX
    OSStatus err;
    ScrapRef scrap;
    err = ::GetScrapByName(kScrapFindScrap, kScrapClearNamedScrap, &scrap);
    if (err == noErr) {
        ::PutScrapFlavor(scrap, kScrapFlavorTypeUnicode, kScrapFlavorMaskNone,
        (mSearchString.Length()*2), aSearchString);
    }
#endif
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

// Same as the tail-end of nsEventStateManager::FocusElementButNotDocument.
// Used here because nsEventStateManager::MoveFocusToCaret() doesn't
// support text input controls.
static void
FocusElementButNotDocument(nsIDocument* aDocument, nsIContent* aContent)
{
  nsIFocusController *focusController = nsnull;
  nsCOMPtr<nsPIDOMWindow> ourWindow =
    do_QueryInterface(aDocument->GetScriptGlobalObject());
  if (ourWindow)
    focusController = ourWindow->GetRootFocusController();
  if (!focusController)
    return;

  // Get previous focus
  nsCOMPtr<nsIDOMElement> oldFocusedElement;
  focusController->GetFocusedElement(getter_AddRefs(oldFocusedElement));
  nsCOMPtr<nsIContent> oldFocusedContent =
    do_QueryInterface(oldFocusedElement);

  // Notify focus controller of new focus for this document
  nsCOMPtr<nsIDOMElement> newFocusedElement(do_QueryInterface(aContent));
  focusController->SetFocusedElement(newFocusedElement);

  nsIPresShell* presShell = aDocument->GetShellAt(0);
  nsCOMPtr<nsPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));
  nsIEventStateManager* esm = presContext->EventStateManager();

  // Temporarily set esm::mCurrentFocus so that esm::GetContentState() tells 
  // layout system to show focus on this element. 
  esm->SetFocusedContent(aContent);  // Reset back to null at the end.
  aDocument->BeginUpdate(UPDATE_CONTENT_STATE);
  aDocument->ContentStatesChanged(oldFocusedContent, aContent, 
                                  NS_EVENT_STATE_FOCUS);
  aDocument->EndUpdate(UPDATE_CONTENT_STATE);

  // Reset esm::mCurrentFocus = nsnull for this doc, so when this document
  // does get focus next time via preHandleEvent() NS_GOTFOCUS,
  // the old document gets blurred
  esm->SetFocusedContent(nsnull);
}

void nsWebBrowserFind::SetSelectionAndScroll(nsIDOMWindow* aWindow,
                                             nsIDOMRange*  aRange)
{
  nsCOMPtr<nsIDOMDocument> domDoc;    
  aWindow->GetDocument(getter_AddRefs(domDoc));
  if (!domDoc) return;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  nsIPresShell* presShell = doc->GetShellAt(0);
  if (!presShell) return;

  // since the match could be an anonymous textnode inside a
  // <textarea> or text <input>, we need to get the outer frame
  nsIFrame *frame = nsnull;
  nsITextControlFrame *tcFrame = nsnull;
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  for ( ; content; content = content->GetParent()) {
    if (!content->IsNativeAnonymous()) {
      presShell->GetPrimaryFrameFor(content, &frame);
      if (!frame)
        return;
      CallQueryInterface(frame, &tcFrame);

      break;
    }
  }

  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsISelectionController> selCon;
  if (!tcFrame) {
    selCon = do_QueryInterface(presShell);
  }
  else {
    tcFrame->GetSelectionContr(getter_AddRefs(selCon));
  }

  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
    getter_AddRefs(selection));
  if (selection) {
    selection->RemoveAllRanges();
    selection->AddRange(aRange);

    if (tcFrame) {
      FocusElementButNotDocument(doc, content);
    }
    else {
      nsCOMPtr<nsPresContext> presContext;
      presShell->GetPresContext(getter_AddRefs(presContext));
      PRBool isSelectionWithFocus;
      presContext->EventStateManager()->
        MoveFocusToCaret(PR_TRUE, &isSelectionWithFocus);
    }

    // Scroll if necessary to make the selection visible:
    // Must be the last thing to do - bug 242056
    selCon->ScrollSelectionIntoView
      (nsISelectionController::SELECTION_NORMAL,
       nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
  }
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

    PRUint32 childCount = bodyContent->GetChildCount();

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
                                  nsISelection* aSel,
                                  PRBool aWrap)
{
    NS_ENSURE_ARG_POINTER(aSel);

    // There is a selection.
    PRInt32 count = -1;
    nsresult rv = aSel->GetRangeCount(&count);
    if (count < 1)
        return SetRangeAroundDocument(aSearchRange, aStartPt, aEndPt, aDoc);

    // Need bodyNode, for the start/end of the document
    nsCOMPtr<nsIDOMNode> bodyNode;
    rv = GetRootNode(aDoc, getter_AddRefs(bodyNode));
    nsCOMPtr<nsIContent> bodyContent (do_QueryInterface(bodyNode));
    NS_ENSURE_ARG_POINTER(bodyContent);

    PRUint32 childCount = bodyContent->GetChildCount();

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
        aSel->GetRangeAt(count-1, getter_AddRefs(range));
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
        aSel->GetRangeAt(0, getter_AddRefs(range));
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
        aSel->GetRangeAt(count-1, getter_AddRefs(range));
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
        aSel->GetRangeAt(0, getter_AddRefs(range));
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
    mCurrentSearchFrame = do_GetWeakReference(aCurrentSearchFrame);
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
    mRootSearchFrame = do_GetWeakReference(aRootSearchFrame);
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

    nsCOMPtr<nsIDOMDocument> domDoc;    
    nsresult rv = aWindow->GetDocument(getter_AddRefs(domDoc));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!domDoc) return NS_ERROR_FAILURE;

    // Do security check, to ensure that the frame we're searching
    // is from the same origin as the frame from which the Find is
    // being run.

    // get a uri for the window
    nsCOMPtr<nsIDocument> theDoc = do_QueryInterface(domDoc);
    if (!theDoc) return NS_ERROR_FAILURE;

    nsIURI *docURI = theDoc->GetDocumentURI();
    NS_ENSURE_TRUE(docURI, NS_ERROR_FAILURE);

    // Get the security manager and do the same-origin check
    nsCOMPtr<nsIScriptSecurityManager> secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = secMan->CheckSameOrigin(nsnull, docURI);
    if (NS_FAILED(rv)) return rv;

    if (!mFind)
        mFind = do_CreateInstance(NS_FIND_CONTRACTID, &rv);

    (void) mFind->SetCaseSensitive(mMatchCase);
    (void) mFind->SetFindBackwards(mFindBackwards);

    // XXX Make and set a line breaker here, once that's implemented.
    (void) mFind->SetWordBreaker(0);

    nsCOMPtr<nsISelection> sel;
    GetFrameSelection(aWindow, getter_AddRefs(sel));
    NS_ENSURE_ARG_POINTER(sel);

    nsCOMPtr<nsIDOMRange> searchRange (do_CreateInstance(kRangeCID));
    NS_ENSURE_ARG_POINTER(searchRange);
    nsCOMPtr<nsIDOMRange> startPt (do_CreateInstance(kRangeCID));
    NS_ENSURE_ARG_POINTER(startPt);
    nsCOMPtr<nsIDOMRange> endPt (do_CreateInstance(kRangeCID));
    NS_ENSURE_ARG_POINTER(endPt);

    nsCOMPtr<nsIDOMRange> foundRange;

    // If !aWrapping, search from selection to end
    if (!aWrapping)
        rv = GetSearchLimits(searchRange, startPt, endPt, domDoc, sel,
                             PR_FALSE);

    // If aWrapping, search the part of the starting frame
    // up to the point where we left off.
    else
        rv = GetSearchLimits(searchRange, startPt, endPt, domDoc, sel,
                             PR_TRUE);

    NS_ENSURE_SUCCESS(rv, rv);

    rv =  mFind->Find(mSearchString.get(), searchRange, startPt, endPt,
                      getter_AddRefs(foundRange));

    if (NS_SUCCEEDED(rv) && foundRange)
    {
        *aDidFind = PR_TRUE;
        sel->RemoveAllRanges();
        SetSelectionAndScroll(aWindow, foundRange);
    }

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

void
nsWebBrowserFind::GetFrameSelection(nsIDOMWindow* aWindow, 
                                    nsISelection** aSel)
{
  *aSel = nsnull;

  nsCOMPtr<nsIDOMDocument> domDoc;    
  aWindow->GetDocument(getter_AddRefs(domDoc));
  if (!domDoc) return;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  nsIPresShell* presShell = doc->GetShellAt(0);
  if (!presShell) return;

  // text input controls have their independent selection controllers
  // that we must use when they have focus.

  nsCOMPtr<nsPresContext> presContext;
  presShell->GetPresContext(getter_AddRefs(presContext));

  nsIFrame *frame = nsnull;
  presContext->EventStateManager()->GetFocusedFrame(&frame);
  if (!frame) {
    nsCOMPtr<nsPIDOMWindow> ourWindow = 
      do_QueryInterface(doc->GetScriptGlobalObject());
    if (ourWindow) {
      nsIFocusController *focusController =
          ourWindow->GetRootFocusController();
      if (focusController) {
        nsCOMPtr<nsIDOMElement> focusedElement;
        focusController->GetFocusedElement(getter_AddRefs(focusedElement));
        if (focusedElement) {
            nsCOMPtr<nsIContent> content(do_QueryInterface(focusedElement));
            presShell->GetPrimaryFrameFor(content, &frame);
        }
      }
    }
  }

  nsCOMPtr<nsISelectionController> selCon;
  if (frame) {
    frame->GetSelectionController(presContext, getter_AddRefs(selCon));
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSel);
    if (*aSel) {
      PRInt32 count = -1;
      (*aSel)->GetRangeCount(&count);
      if (count > 0) {
        return;
      }
      NS_RELEASE(*aSel);
    }
  }

  selCon = do_QueryInterface(presShell);
  selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSel);
}

nsresult nsWebBrowserFind::ClearFrameSelection(nsIDOMWindow *aWindow)
{
    NS_ENSURE_ARG(aWindow);
    nsCOMPtr<nsISelection> selection;
    GetFrameSelection(aWindow, getter_AddRefs(selection));
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
    nsIFocusController *focusController = nsnull;
    if (ourWindow)
        focusController = ourWindow->GetRootFocusController();
    if (focusController)
    {
        nsCOMPtr<nsIDOMWindowInternal> windowInt = do_QueryInterface(aFoundWindow);
        focusController->SetFocusedWindow(windowInt);
        mLastFocusedWindow = do_GetWeakReference(aFoundWindow);
    }

    return NS_OK;
}

/*---------------------------------------------------------------------------

  GetDocShellFromWindow

  Utility method. This will always return nsnull if no docShell
  is returned. Oh why isn't there a better way to do this?
----------------------------------------------------------------------------*/
nsIDocShell *
nsWebBrowserFind::GetDocShellFromWindow(nsIDOMWindow *inWindow)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptGO(do_QueryInterface(inWindow));
  if (!scriptGO) return nsnull;

  return scriptGO->GetDocShell();
}

