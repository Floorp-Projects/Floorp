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
 */

#include "nsWebBrowserFind.h"

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsITextServicesDocument.h"
#include "nsTextServicesCID.h"
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
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIFocusController.h"
#include "nsISelection.h"
#include "nsReadableUtils.h"

#if DEBUG
#include "nsIWebNavigation.h"
#include "nsXPIDLString.h"
#endif

static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);

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
    NS_INIT_ISUPPORTS();
}

nsWebBrowserFind::~nsWebBrowserFind()
{
}

NS_IMPL_ISUPPORTS2(nsWebBrowserFind, nsIWebBrowserFind, nsIWebBrowserFindInFrames);


/* boolean findNext (); */
NS_IMETHODIMP nsWebBrowserFind::FindNext(PRBool *outDidFind)
{
    NS_ENSURE_ARG_POINTER(outDidFind);
    *outDidFind = PR_FALSE;

    NS_ENSURE_TRUE(CanFindNext(), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mCurrentSearchFrame);
    NS_ENSURE_TRUE(searchFrame, NS_ERROR_NOT_INITIALIZED);
    
    // first, look in the current frame. If found, return.
    nsresult    rv = SearchInFrame(searchFrame, outDidFind);
    if (NS_FAILED(rv)) return rv;
    if (*outDidFind)
        return OnFind(searchFrame);     // we are done

    // if we are not searching other frames, return
    if (!mSearchSubFrames && !mSearchParentFrames)
        return NS_OK;

    nsCOMPtr<nsIDOMWindow> rootFrame = do_QueryReferent(mRootSearchFrame);
    NS_ENSURE_TRUE(searchFrame, NS_ERROR_NOT_INITIALIZED);
    
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

            rv = SearchInFrame(searchFrame, outDidFind);
            if (NS_FAILED(rv)) return rv;
            if (*outDidFind)
                return OnFind(searchFrame);     // we are done
                
            OnEndSearchFrame(searchFrame);
        }
        
        if (curItem.get() == startingItem.get())
            doFind = PR_TRUE;       // start looking in frames after this one
    };


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
            // ideally, we should search the part of the starting frame up
            // to the point where we left off. We could do this by keeping
            // its nsIFindAndReplace around.        
            break;
        }

        searchFrame = do_GetInterface(curItem, &rv);
        if (NS_FAILED(rv)) break;
        
        OnStartSearchFrame(searchFrame);

        rv = SearchInFrame(searchFrame, outDidFind);
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

/*
    This method handles finding in a single window (aka frame).

*/
nsresult nsWebBrowserFind::SearchInFrame(nsIDOMWindow* aWindow, PRBool* aDidFind)
{
    NS_ENSURE_ARG(aWindow);
    
    nsresult rv = NS_OK;

    // if this is a different frame to last time, throw away the mTSFind
    // and make a new one. The nsIFindAndReplace is *not* stateless;
    // it remembers the last search offset etc.
    nsCOMPtr<nsIDOMWindow> searchFrame = do_QueryReferent(mCurrentSearchFrame);    
    if (mTSFind && (searchFrame.get() != aWindow))
        mTSFind = nsnull;       // throw away old one, if any

    if (!mTSFind)
    {
        mTSFind = do_CreateInstance(NS_FINDANDREPLACE_CONTRACTID, &rv);
        if (NS_FAILED(rv))
            return rv;
    }

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

    nsCOMPtr<nsITextServicesDocument> txtDoc;
    rv = MakeTSDocument(aWindow, getter_AddRefs(txtDoc));
    if (NS_FAILED(rv) || !txtDoc)
        return rv;
    
    (void) mTSFind->SetCaseSensitive(mMatchCase);
    (void) mTSFind->SetFindBackwards(mFindBackwards);
    (void) mTSFind->SetWrapFind(mWrapFind);
    (void) mTSFind->SetEntireWord(mEntireWord);

    rv = mTSFind->SetTsDoc(txtDoc);
    if (NS_FAILED(rv))
        return rv;

    rv =  mTSFind->Find(mSearchString.get(), aDidFind);

    mTSFind->SetTsDoc(nsnull);

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

nsresult nsWebBrowserFind::MakeTSDocument(nsIDOMWindow* aWindow, nsITextServicesDocument** aDoc)
{
    NS_ENSURE_ARG(aWindow);
    NS_ENSURE_ARG_POINTER(aDoc);

    nsresult rv;
    *aDoc = NULL;

    nsCOMPtr<nsITextServicesDocument>  tempDoc(do_CreateInstance(kCTextServicesDocumentCID, &rv));
    if (NS_FAILED(rv) || !tempDoc)
        return rv;

    nsCOMPtr<nsIDocShell> docShell;
    rv = GetDocShellFromWindow(aWindow, getter_AddRefs(docShell));
    if (NS_FAILED(rv) || !docShell)
        return NS_ERROR_FAILURE;
	
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    if (!presShell)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> domDoc;    
    rv = aWindow->GetDocument(getter_AddRefs(domDoc));
    if (!domDoc)
        return NS_ERROR_FAILURE;

    rv = tempDoc->InitWithDocument(domDoc, presShell);
    if (NS_FAILED(rv))
        return rv;

    // Return the resulting text services document.
    *aDoc = tempDoc;
    NS_IF_ADDREF(*aDoc);
  
    return rv;
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

