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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 */

/*
 * nsActions.cpp
 */

#include "nsActions.h"
#include "nsCOMPtr.h"
#include "nsIContentViewer.h"
#include "nsIBaseWindow.h"
#include "nsISHEntry.h"
#include "nsIURI.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIFindComponent.h"
#include "nsISearchContext.h"
#include "nsIContentViewerEdit.h"

#include "ns_util.h"
#include "rdf_util.h"

#include "nsEmbedAPI.h"  // for NS_TermEmbedding

#include "nsRDFCID.h" // for NS_RDFCONTAINER_CID

static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);

//
// Local function prototypes
//

/**

 * pull the int for the field nativeEnum from the java object obj.

 */

jint getNativeEnumFromJava(JNIEnv *env, jobject obj, jint nativeRDFNode);

void *          handleEvent     (PLEvent * event);
void            destroyEvent    (PLEvent * event);


void *
handleEvent (PLEvent * event)
{
        nsActionEvent   * actionEvent = (nsActionEvent*) event->owner;
        void            * result = nsnull;

        result = actionEvent->handleEvent();

        return result;
} // handleEvent()


void
destroyEvent (PLEvent * event)
{
        nsActionEvent * actionEvent = (nsActionEvent*) event->owner;

        if (actionEvent != nsnull) {
                actionEvent->destroyEvent();
        }
} // destroyEvent()



/*
 * nsActionEvent
 */

nsActionEvent::nsActionEvent ()
{
        PL_InitEvent(&mEvent, this,
                (PLHandleEventProc ) ::handleEvent,
                (PLDestroyEventProc) ::destroyEvent);
}

wsHistoryActionEvent::wsHistoryActionEvent(nsISHistory *yourHistory)
{
    mHistory = yourHistory;
}

/*
 * wsResizeEvent
 */

wsResizeEvent::wsResizeEvent(nsIBaseWindow* baseWindow, PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
        mLeft(x),
        mBottom(y),
        mWidth(w),
        mHeight(h)
{
}


void *
wsResizeEvent::handleEvent ()
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mBaseWindow) {

        rv = mBaseWindow->SetPositionAndSize(mLeft, mBottom, mWidth, mHeight, 
                                            PR_TRUE);
        
        
        return (void *) rv;
    }
    return nsnull;
} // handleEvent()




/*
 * wsLoadURLEvent
 */

wsLoadURLEvent::wsLoadURLEvent(nsIWebNavigation* webNavigation, PRUnichar * urlString, PRInt32 urlLength) :
        nsActionEvent(),
        mWebNavigation(webNavigation),
        mURL(nsnull)
{
        mURL = new nsString(urlString, urlLength);
}


void *
wsLoadURLEvent::handleEvent ()
{
  if (mWebNavigation && mURL) {
      nsresult rv = mWebNavigation->LoadURI(mURL->GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
  }
  return nsnull;
} // handleEvent()


wsLoadURLEvent::~wsLoadURLEvent ()
{
  if (mURL != nsnull)
    delete mURL;
}



/*
 * wsStopEvent
 */

wsStopEvent::wsStopEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
        mWebNavigation(webNavigation)
{
}


void *
wsStopEvent::handleEvent ()
{
        if (mWebNavigation) {
                nsresult rv = mWebNavigation->Stop();
        }
        return nsnull;
} // handleEvent()



/*
 * wsShowEvent
 */

wsShowEvent::wsShowEvent(nsIBaseWindow* baseWindow, PRBool state) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
	mState(state)
{
}


void *
wsShowEvent::handleEvent ()
{
        if (mBaseWindow) {
                mBaseWindow->SetVisibility(mState);
        }
        return nsnull;
} // handleEvent()



/*
 * wsHideEvent
 */

wsHideEvent::wsHideEvent(nsIBaseWindow* baseWindow) :
        nsActionEvent(),
        mBaseWindow(baseWindow)
{
}


void *
wsHideEvent::handleEvent ()
{
        if (mBaseWindow) {
                mBaseWindow->SetVisibility(PR_FALSE);
        }
        return nsnull;
} // handleEvent()



/*
 * wsMoveToEvent
 */

wsMoveToEvent::wsMoveToEvent(nsIBaseWindow* baseWindow, PRInt32 x, PRInt32 y) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
        mX(x),
        mY(y)
{
}


void *
wsMoveToEvent::handleEvent ()
{
        if (mBaseWindow) {
               nsresult rv = mBaseWindow->SetPosition(mX, mY);
        }
        return nsnull;
} // handleEvent()


/*
 * wsSetFocusEvent
 */

wsSetFocusEvent::wsSetFocusEvent(nsIBaseWindow* baseWindow) :
        nsActionEvent(),
        mBaseWindow(baseWindow)
{
}


void *
wsSetFocusEvent::handleEvent ()
{
        if (mBaseWindow) {
               nsresult rv = mBaseWindow->SetFocus();
        }
        return nsnull;
} // handleEvent()



/*
 * wsRemoveFocusEvent
 */

wsRemoveFocusEvent::wsRemoveFocusEvent(nsIBaseWindow* baseWindow) :
        nsActionEvent(),
        mBaseWindow(baseWindow)
{
}


void *
wsRemoveFocusEvent::handleEvent ()
{
        if (mBaseWindow) {
	  //PENDING (Ashu) : No removeFocus functionality provided in M15
	  //                nsresult rv = mWebShell->RemoveFocus();
        }
        return nsnull;
} // handleEvent()



/*
 * wsRepaintEvent
 */

wsRepaintEvent::wsRepaintEvent(nsIBaseWindow* baseWindow, PRBool forceRepaint) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
        mForceRepaint(forceRepaint)
{
}


void *
wsRepaintEvent::handleEvent ()
{
        if (mBaseWindow) {
               nsresult rv = mBaseWindow->Repaint(mForceRepaint);
        }
        return nsnull;
} // handleEvent()



/*
 * wsCanBackEvent
 */

wsCanBackEvent::wsCanBackEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
	mWebNavigation(webNavigation)
{

}

void *
wsCanBackEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
        nsresult rv;
        PRBool canGoBack;
        
        rv = mWebNavigation->GetCanGoBack(&canGoBack);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        result = (void *)canGoBack;
    }
    return result;
} // handleEvent()

/*
 * wsCanForwardEvent
 */

wsCanForwardEvent::wsCanForwardEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
	mWebNavigation(webNavigation)
{

}


void *
wsCanForwardEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
        nsresult rv;
        PRBool canGoForward;
        
        rv = mWebNavigation->GetCanGoForward(&canGoForward);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        result = (void *)canGoForward;
        
    }
    return result;
} // handleEvent()



/*
 * wsBackEvent
 */

wsBackEvent::wsBackEvent(nsIWebNavigation* webNavigation) :
  nsActionEvent(),
  mWebNavigation(webNavigation)
{
}


void *
wsBackEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
        nsresult rv = mWebNavigation->GoBack();
        
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsForwardEvent
 */

wsForwardEvent::wsForwardEvent(nsIWebNavigation* webNavigation) :
  nsActionEvent(),
  mWebNavigation(webNavigation)     
{
}


void *
wsForwardEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
                
        nsresult rv = mWebNavigation->GoForward();
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGoToEvent
 */

wsGoToEvent::wsGoToEvent(nsIWebNavigation* webNavigation, PRInt32 historyIndex) :
        nsActionEvent(),
        mWebNavigation(webNavigation), mHistoryIndex(historyIndex)
{
}


void *
wsGoToEvent::handleEvent ()
{
    void *result = nsnull;
    nsresult rv = nsnull;
    if (mWebNavigation) {
      //PENDING (Ashu) : GoTo Functionality seems to be missing in M15
      //        nsresult rv = mHistory->Goto(mHistoryIndex, mWebShell, PR_TRUE);
      result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryLengthEvent
 */

wsGetHistoryLengthEvent::wsGetHistoryLengthEvent(nsISHistory * sHistory) :
     nsActionEvent(),
     mHistory(sHistory)
{
}


void *
wsGetHistoryLengthEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        PRInt32 historyLength = 0;
        
        nsresult rv = mHistory->GetCount(&historyLength);
        result = (void *) historyLength;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryIndexEvent
 */

wsGetHistoryIndexEvent::wsGetHistoryIndexEvent(nsISHistory * sHistory) :
        nsActionEvent(),
	mHistory(sHistory)
{
}


void *
wsGetHistoryIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        PRInt32 historyIndex = 0;

        nsresult rv = mHistory->GetIndex(&historyIndex);
        result = (void *) historyIndex;
    }
    return result;
} // handleEvent()



/*
 * wsGetURLEvent
 */

wsGetURLEvent::wsGetURLEvent(nsISHistory * sHistory) :
        nsActionEvent(),
	mHistory(sHistory)
{
}


void *
wsGetURLEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        
        
        PRInt32 currentIndex;
        char *currentURL = nsnull;
        nsresult rv;

        rv = mHistory->GetIndex(&currentIndex);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        nsISHEntry * Entry;
        rv = mHistory->GetEntryAtIndex(currentIndex, PR_FALSE, &Entry);

        if (NS_FAILED(rv)) {
            return result;
        }

	nsIURI * URI;
	rv = Entry->GetURI(&URI);

	if (NS_FAILED(rv)) {
            return result;
        }
	
	rv = URI->GetSpec(&currentURL);
	if (NS_FAILED(rv)) {
            return result;
        }
        
        result = (void *) currentURL;
    }
    return result;
} // handleEvent()

/*
 * wsGetURLForIndexEvent
 */

wsGetURLForIndexEvent::wsGetURLForIndexEvent(nsISHistory * sHistory, 
                                             PRInt32 historyIndex) :
  nsActionEvent(),
  mHistory(sHistory),
  mHistoryIndex(historyIndex)
{
}


void *
wsGetURLForIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        nsresult rv;
        char *indexURL = nsnull;

	nsISHEntry * Entry;
        rv = mHistory->GetEntryAtIndex(mHistoryIndex, PR_FALSE, &Entry);
        if (NS_FAILED(rv)) {
            return result;
        }

	nsIURI * URI;
	rv = Entry->GetURI(&URI);

	if (NS_FAILED(rv)) {
            return result;
        }
	
	rv = URI->GetSpec(&indexURL);
	if (NS_FAILED(rv)) {
            return result;
        }	

        result = (void *) indexURL;
    }
    return result;
} // handleEvent()



// Added by Mark Goddard OTMP 9/2/1999

/*
 * wsRefreshEvent
 */

wsRefreshEvent::wsRefreshEvent(nsIWebNavigation* webNavigation, PRInt32 reloadType) :
        nsActionEvent(),
        mWebNavigation(webNavigation),
	mReloadType(reloadType)
{

}


void *
wsRefreshEvent::handleEvent ()
{
        if (mWebNavigation) {
                nsresult rv = mWebNavigation->Reload(mReloadType);
                return (void *) rv;
        }
        return nsnull;
} // handleEvent()



wsViewSourceEvent::wsViewSourceEvent(nsIDocShell* docShell, PRBool viewMode) :
    nsActionEvent(),
    mDocShell(docShell),
    mViewMode(viewMode)
{
}

void *
wsViewSourceEvent::handleEvent ()
{
    if(mDocShell) {
        if(mViewMode) {
            nsresult rv = mDocShell->SetViewMode(nsIDocShell::viewSource);
            return (void *) rv;
        }
        else
            {
                nsresult rv = mDocShell->SetViewMode(nsIDocShell::viewNormal);
                return (void *) rv;
            }
    }
    return nsnull;
}

wsAddDocLoaderObserverEvent::wsAddDocLoaderObserverEvent(nsIDocShell* yourDocShell,
                                                         nsIDocumentLoaderObserver *yourObserver) :
        nsActionEvent(),
        mDocShell(yourDocShell), mDocObserver(yourObserver)
{
}

void *
wsAddDocLoaderObserverEvent::handleEvent ()
{
    void *result = nsnull;
    
    if (mDocShell && mDocObserver) {
        
        nsresult rv = mDocShell->SetDocLoaderObserver(mDocObserver);
        result = (void *) rv;
    }
    return result;
} // handleEvent()

wsDeallocateInitContextEvent::wsDeallocateInitContextEvent(WebShellInitContext* yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsDeallocateInitContextEvent::handleEvent ()
{
    void *result = nsnull;
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }

    mInitContext->parentHWnd = nsnull;

    // make sure the webBrowser's contentListener doesn't have a ref to
    // CBrowserContainer
    mInitContext->webBrowser->SetParentURIContentListener(nsnull);
    mInitContext->webBrowser = nsnull;

    // make sure the webShell's container is set to null.
    // get the webShell from the docShell
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mInitContext->docShell));
    if (webShell) {
        webShell->SetContainer(nsnull);
    }

    // make sure the docShell's TreeOwner is set to null
    // get the docShellTreeItem from the docShell
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mInitContext->docShell));
    if (docShellAsItem) {
        docShellAsItem->SetTreeOwner(nsnull);
    }

    mInitContext->docShell->SetDocLoaderObserver(nsnull);

    mInitContext->docShell = nsnull;

    // PENDING(edburns): this is a leak.  For some reason, webShell's
    // refcount is two.  I believe it should be one.
    // see http://bugzilla.mozilla.org/show_bug.cgi?id=38271

    mInitContext->webShell = nsnull;
    mInitContext->webNavigation = nsnull;
    mInitContext->presShell = nsnull;
    mInitContext->sHistory = nsnull;
    mInitContext->baseWindow = nsnull;

    mInitContext->embeddedThread = nsnull;
    mInitContext->env = nsnull;
    if (nsnull != mInitContext->nativeEventThread) {
        ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), 
                               mInitContext->nativeEventThread);
        mInitContext->nativeEventThread = nsnull;
    }
    mInitContext->stopThread = -1;
    mInitContext->initComplete = FALSE;
    mInitContext->initFailCode = 0;
    mInitContext->x = -1;
    mInitContext->y = -1;
    mInitContext->w = -1;
    mInitContext->h = -1;    
    mInitContext->gtkWinPtr = nsnull;
    mInitContext->searchContext = nsnull;

    // make sure we aren't listening anymore.  This needs to be done
    // before currentDocument = nsnull
    mInitContext->browserContainer->RemoveAllListeners();
    
    mInitContext->currentDocument = nsnull;
    mInitContext->browserContainer = nsnull;
    util_DeallocateShareInitContext(&(mInitContext->shareContext));

    //  delete mInitContext;
    NS_TermEmbedding();
    return (void *) NS_OK;
} // handleEvent()

wsInitBookmarksEvent::wsInitBookmarksEvent(WebShellInitContext* yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsInitBookmarksEvent::handleEvent ()
{
    void *result = nsnull;
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    rv = rdf_InitRDFUtils();
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: can't initialize RDF Utils");
        return (void *) result;
    }
    result = (void *) kNC_BookmarksRoot.get();

    return result;
} // handleEvent()


wsNewRDFNodeEvent::wsNewRDFNodeEvent(WebShellInitContext* yourInitContext,
                                     const char * yourUrlString,
                                     PRBool yourIsFolder) :
        nsActionEvent(),
        mInitContext(yourInitContext), mUrlString(yourUrlString),
        mIsFolder(yourIsFolder)
{
}

void *
wsNewRDFNodeEvent::handleEvent ()
{
    void *result = nsnull;
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    nsCOMPtr<nsIRDFResource> newNode;
	nsCAutoString uri("NC:BookmarksRoot");
    JNIEnv *env = (JNIEnv*) JNU_GetEnv(gVm, JNI_VERSION);
    
    const char *url = mUrlString;
	uri.Append("#$");
	uri.Append(url);
    PRUnichar *uriUni = uri.ToNewUnicode();
    
    rv = gRDF->GetUnicodeResource(uriUni, getter_AddRefs(newNode));
    nsCRT::free(uriUni);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create new nsIRDFResource.");
        return result;
    }

    if (mIsFolder) {
        rv = gRDFCU->MakeSeq(gBookmarksDataSource, newNode, nsnull);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Exception: unable to make new folder as a sequence.");
            return result;
        }
        rv = gBookmarksDataSource->Assert(newNode, kRDF_type, 
                                          kNC_Folder, PR_TRUE);
        if (rv != NS_OK) {
            ::util_ThrowExceptionToJava(env, "Exception: unable to mark new folder as folder.");
            
            return result;
        }
    }

    /*

     * Do the AddRef here.

     */

    result = (void *)newNode.get();
    ((nsISupports *)result)->AddRef();

    return result;
} // handleEvent()

wsRDFIsContainerEvent::wsRDFIsContainerEvent(WebShellInitContext* yourInitContext, 
                                             PRUint32 yourNativeRDFNode) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode)
{
}

void *
wsRDFIsContainerEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) mNativeRDFNode;
    nsCOMPtr<nsIRDFResource> nodeResource;
    nsresult rv;
    jboolean result = JNI_FALSE;
    PRBool prBool;
    
    rv = node->QueryInterface(NS_GET_IID(nsIRDFResource), 
                              getter_AddRefs(nodeResource));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeIsContainer: nativeRDFNode is not an RDFResource.");
        return nsnull;
    }
    rv = gRDFCU->IsContainer(gBookmarksDataSource, nodeResource, 
                             &prBool);
    result = (prBool == PR_FALSE) ? JNI_FALSE : JNI_TRUE;
    
    return (void *) result;
} // handleEvent()

void *
wsFindEvent::handleEvent ()
{
    void *result = nsnull;
    
    if (mFindComponent && mSearchContext) {
        PRBool found = PR_TRUE;
        nsresult rv = mFindComponent->FindNext(mSearchContext, &found);
        result = (void *) rv;
    }
    return result;
}

wsFindEvent::wsFindEvent(nsIFindComponent * findcomponent, nsISearchContext * srchcontext) :
        nsActionEvent(),
        mFindComponent(findcomponent),
	mSearchContext(srchcontext)
{
}

wsRDFGetChildAtEvent::wsRDFGetChildAtEvent(WebShellInitContext* yourInitContext, 
                                           PRUint32 yourNativeRDFNode, 
                                           PRUint32 yourChildIndex) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode),
        mChildIndex(yourChildIndex)
{
}

void *
wsRDFGetChildAtEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    jint result = -1;
    nsresult rv;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mNativeRDFNode;
    nsCOMPtr<nsIRDFResource> child;

    rv = rdf_getChildAt(mChildIndex, parent, getter_AddRefs(child));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildAt: Can't get child.");
        return nsnull;
    }
    result = (jint)child.get();
    ((nsISupports *)result)->AddRef();
    return (void *) result;
} // handleEvent()

wsRDFGetChildCountEvent::wsRDFGetChildCountEvent(WebShellInitContext* yourInitContext, 
                                                 PRUint32 yourNativeRDFNode) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode)
{
}

void *
wsRDFGetChildCountEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    jint result = -1;
    PRInt32 count;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mNativeRDFNode;

    rv = rdf_getChildCount(parent, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildCount: Can't get child count.");
        return nsnull;
    }
    result = (jint)count;
  return (void *) result;
} // handleEvent()

wsRDFGetChildIndexEvent::wsRDFGetChildIndexEvent(WebShellInitContext* yourInitContext, 
                                                 PRUint32 yourNativeRDFNode, 
                                                 PRUint32 yourChildRDFNode) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode), 
        mChildRDFNode(yourChildRDFNode)
{
}

void *
wsRDFGetChildIndexEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    jint result = -1;
    PRInt32 index;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mNativeRDFNode;
    nsCOMPtr<nsIRDFResource> child = (nsIRDFResource *) mChildRDFNode;
    
    rv = rdf_getIndexOfChild(parent, child, &index);
    result = (jint) index;
    
    return (void *) result;
} // handleEvent()

void *
wsSelectAllEvent::handleEvent ()
{
    void *result = nsnull;
    
    if (mContentViewerEdit) {
        nsresult rv = mContentViewerEdit->SelectAll();
        result = (void *) rv;
    }
    return result;
}

wsSelectAllEvent::wsSelectAllEvent(nsIContentViewerEdit * contentViewerEdit) :
        nsActionEvent(),
        mContentViewerEdit(contentViewerEdit)
{
}

wsRDFToStringEvent::wsRDFToStringEvent(WebShellInitContext* yourInitContext, 
                                       PRUint32 yourNativeRDFNode) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode)
{
}

void *
wsRDFToStringEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<nsIRDFResource> currentResource = 
        (nsIRDFResource *) mNativeRDFNode;
    nsCOMPtr<nsIRDFNode> node;
    nsCOMPtr<nsIRDFLiteral> literal;
    jstring result = nsnull;
    PRBool isContainer = PR_FALSE;
    nsresult rv;
    const PRUnichar *textForNode = nsnull;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    rv = gRDFCU->IsContainer(gBookmarksDataSource, currentResource, 
                             &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeToString: Can't tell if RDFResource is container.");
        return nsnull;
    }
    
    if (isContainer) {
        // It's a bookmarks folder
        rv = gBookmarksDataSource->GetTarget(currentResource,
                                             kNC_Name, PR_TRUE, 
                                             getter_AddRefs(node));
        // get the name of the folder
        if (rv == 0) {
            // if so, make sure it's an nsIRDFLiteral
            rv = node->QueryInterface(NS_GET_IID(nsIRDFLiteral), 
                                      getter_AddRefs(literal));
            if (NS_SUCCEEDED(rv)) {
                rv = literal->GetValueConst(&textForNode);
            }
            else {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("nativeToString: node is not an nsIRDFLiteral.\n"));
                }
            }
        }
    }
    else {
        // It's a bookmark or a Separator
        rv = gBookmarksDataSource->GetTarget(currentResource,
                                             kNC_URL, PR_TRUE, 
                                             getter_AddRefs(node));
        // See if it has a Name
        if (0 != rv) {
            rv = gBookmarksDataSource->GetTarget(currentResource,
                                                 kNC_Name, PR_TRUE, 
                                                 getter_AddRefs(node));
        }
        
        if (0 == rv) {
            rv = node->QueryInterface(NS_GET_IID(nsIRDFLiteral), 
                                      getter_AddRefs(literal));
            if (NS_SUCCEEDED(rv)) {
                // get the value of the literal
                rv = literal->GetValueConst(&textForNode);
                if (NS_FAILED(rv)) {
                    if (prLogModuleInfo) {
                        PR_LOG(prLogModuleInfo, 3, 
                               ("nativeToString: node doesn't have a value.\n"));
                    }
                }
            }
            else {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("nativeToString: node is not an nsIRDFLiteral.\n"));
                }
            }
        }
        else {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("nativeToString: node doesn't have a URL.\n"));
            }
        }
    }

    if (nsnull != textForNode) {
        nsString * string = new nsString(textForNode);
        int length = 0;
        if (nsnull != string) {
            length = string->Length();
        }

        result = ::util_NewString(env, (const jchar *) textForNode, length);
    }
    else {
        result = ::util_NewStringUTF(env, "");
    }

    return (void *) result;
} // handleEvent()

wsRDFInsertElementAtEvent::wsRDFInsertElementAtEvent(WebShellInitContext* yourInitContext, 
                                                     PRUint32 yourParentRDFNode, 
                                                     PRUint32 yourChildRDFNode, 
                                                     PRUint32 yourChildIndex) :
        nsActionEvent(),
        mInitContext(yourInitContext), mParentRDFNode(yourParentRDFNode),
        mChildRDFNode(yourChildRDFNode), mChildIndex(yourChildIndex)
{
}

void *
wsRDFInsertElementAtEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mParentRDFNode;
    nsCOMPtr<nsIRDFResource> newChild = (nsIRDFResource *) mChildRDFNode;
    nsCOMPtr<nsIRDFContainer> container;
    nsresult rv;
    PRBool isContainer;
    
    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: RDFResource is not a container.");
        return (void *) NS_ERROR_UNEXPECTED;
    }

    PR_ASSERT(gComponentManager);

    // get a container in order to create a child
    rv = gComponentManager->CreateInstance(kRDFContainerCID,
                                           nsnull,
                                           NS_GET_IID(nsIRDFContainer),
                                           getter_AddRefs(container));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create container.");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create container.");
        return (void *) NS_ERROR_UNEXPECTED;
    }

    rv = container->InsertElementAt(newChild, mChildIndex, PR_TRUE);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't insert element into parent container.");
        return (void *) NS_ERROR_UNEXPECTED;
    }

    return (void *) NS_OK;
} // handleEvent()

wsRDFHasMoreElementsEvent::wsRDFHasMoreElementsEvent(WebShellInitContext* yourInitContext,
                                                     PRUint32 yourNativeRDFNode,
                                                     void *yourJobject) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode),
        mJobject(yourJobject)
{
}

void *
wsRDFHasMoreElementsEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    jboolean result = JNI_FALSE;
    PRBool prResult = PR_FALSE;
    // assert -1 != nativeRDFNode
    jint nativeEnum;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    if (-1 == (nativeEnum = getNativeEnumFromJava(env, (jobject) mJobject, 
                                                  mNativeRDFNode))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeHasMoreElements: Can't get nativeEnum from nativeRDFNode.");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    nsCOMPtr<nsISimpleEnumerator> enumerator = (nsISimpleEnumerator *)nativeEnum;
    rv = enumerator->HasMoreElements(&prResult);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeHasMoreElements: Can't ask nsISimpleEnumerator->HasMoreElements().");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    result = (PR_FALSE == prResult) ? JNI_FALSE : JNI_TRUE;
    
    return (void *) result;
    
} // handleEvent()

wsRDFNextElementEvent::wsRDFNextElementEvent(WebShellInitContext* yourInitContext,
                                             PRUint32 yourNativeRDFNode,
                                             void *yourJobject) :
    nsActionEvent(),
    mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode),
    mJobject(yourJobject)
{
}

void *
wsRDFNextElementEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    jint result = -1;
    PRBool hasMoreElements = PR_FALSE;
    // assert -1 != nativeRDFNode
    jint nativeEnum;
    nsCOMPtr<nsISupports> supportsResult;
    nsCOMPtr<nsIRDFNode> nodeResult;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    if (-1 == (nativeEnum = getNativeEnumFromJava(env, (jobject) mJobject, 
                                                  mNativeRDFNode))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNextElement: Can't get nativeEnum from nativeRDFNode.");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    nsCOMPtr<nsISimpleEnumerator> enumerator = (nsISimpleEnumerator *)nativeEnum;
    rv = enumerator->HasMoreElements(&hasMoreElements);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNextElement: Can't ask nsISimpleEnumerator->HasMoreElements().");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    if (!hasMoreElements) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    rv = enumerator->GetNext(getter_AddRefs(supportsResult));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("Exception: nativeNextElement: Can't get next from enumerator.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    // make sure it's an RDFNode
    rv = supportsResult->QueryInterface(NS_GET_IID(nsIRDFNode), 
                                        getter_AddRefs(nodeResult));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("Exception: nativeNextElement: next from enumerator is not an nsIRDFNode.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    result = (jint)nodeResult.get();
    ((nsISupports *)result)->AddRef();
    return (void *) result;
} // handleEvent()

void *
wsCopySelectionEvent::handleEvent ()
{
    void *result = nsnull;
    
    if (mContentViewerEdit) {
        nsresult rv = mContentViewerEdit->CopySelection();
        result = (void *) rv;
    }
    return result;
}

wsCopySelectionEvent::wsCopySelectionEvent(nsIContentViewerEdit * contentViewerEdit) :
        nsActionEvent(),
        mContentViewerEdit(contentViewerEdit)
{
}

wsRDFFinalizeEvent::wsRDFFinalizeEvent(void *yourJobject) :
        nsActionEvent(),
        mJobject(yourJobject)
{
}

void *
wsRDFFinalizeEvent::handleEvent ()
{
    if (!mJobject) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    jint nativeEnum, nativeContainer;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    // release the nsISimpleEnumerator
    if (-1 == (nativeEnum = 
               ::util_GetIntValueFromInstance(env, (jobject) mJobject, 
                                              "nativeEnum"))) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("nativeFinalize: Can't get fieldID for nativeEnum.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<nsISimpleEnumerator> enumerator = 
        (nsISimpleEnumerator *) nativeEnum;
    ((nsISupports *)enumerator.get())->Release();
    
    // release the nsIRDFContainer
    if (-1 == (nativeContainer = 
               ::util_GetIntValueFromInstance(env, (jobject) mJobject, 
                                              "nativeContainer"))) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("nativeFinalize: Can't get fieldID for nativeContainerFieldID.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<nsIRDFContainer> container = 
        (nsIRDFContainer *) nativeContainer;
    ((nsISupports *)container.get())->Release();

    return (void *) NS_OK;
} // handleEvent()

//
// Local functions
//

jint getNativeEnumFromJava(JNIEnv *env, jobject obj, jint nativeRDFNode)
{
    nsresult rv;
    jint result = -1;

    result = ::util_GetIntValueFromInstance(env, obj, "nativeEnum");

    // if the field has been initialized, just return the value
    if (-1 != result) {
        // NORMAL EXIT 1
        return result;
    }

    // else, we need to create the enum
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> nodeResource;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsISimpleEnumerator> enumerator;

    rv = node->QueryInterface(NS_GET_IID(nsIRDFResource), 
                              getter_AddRefs(nodeResource));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("getNativeEnumFromJava: Argument nativeRDFNode isn't an nsIRDFResource.\n"));
        }
        return -1;
    }

    PR_ASSERT(gComponentManager);
    
    // get a container in order to get the enum
    rv = gComponentManager->CreateInstance(kRDFContainerCID,
                                           nsnull,
                                           NS_GET_IID(nsIRDFContainer),
                                           getter_AddRefs(container));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("recursiveResourceTraversal: can't get a new container\n"));
        }
        return -1;
    }
    
    rv = container->Init(gBookmarksDataSource, nodeResource);
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("getNativeEnumFromJava: Can't Init container.\n"));
        }
        return -1;
    }

    rv = container->GetElements(getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("getNativeEnumFromJava: Can't get enumeration from container.\n"));
        }
        return -1;
    }

    // IMPORTANT: Store the enum back into java
    ::util_SetIntValueForInstance(env,obj,"nativeEnum",(jint)enumerator.get());
    // IMPORTANT: make sure it doesn't get deleted when it goes out of scope
    ((nsISupports *)enumerator.get())->AddRef(); 

    // PENDING(edburns): I'm not sure if we need to keep the
    // nsIRDFContainer from being destructed in order to maintain the
    // validity of the nsISimpleEnumerator that came from the container.
    // Just to be safe, I'm doing so.
    ::util_SetIntValueForInstance(env, obj, "nativeContainer", 
                                  (jint) container.get());
    ((nsISupports *)container.get())->AddRef();
    
    // NORMAL EXIT 2
    result = (jint)enumerator.get();    
    return result;
}
