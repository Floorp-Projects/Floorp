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
 *               Jason Mawdsley <jason@macadamian.com>
 *               Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 *               Kyle Yuan <kyle.yuan@sun.com>
 */

/*
 * CurrentPageActionEvents.cpp
 */

#include "CurrentPageActionEvents.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerEdit.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIHistoryEntry.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserPrint.h"
#include "nsIPrintSettings.h"
#include "nsIDOMWindow.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMNode.h"
#include "nsCRT.h"

wsCopySelectionEvent::wsCopySelectionEvent(NativeBrowserControl *yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsCopySelectionEvent::handleEvent ()
{
    void *result = nsnull;

    if (mInitContext) {
        nsIContentViewer* contentViewer ;
        nsresult rv = nsnull;

        rv = mInitContext->docShell->GetContentViewer(&contentViewer);
        if (NS_FAILED(rv) || contentViewer==nsnull )  {
            return (void *) rv;
        }

        nsCOMPtr<nsIContentViewerEdit> contentViewerEdit(do_QueryInterface(contentViewer));

        rv = contentViewerEdit->CopySelection();
        result = (void *) rv;
    }
    return result;
}

wsGetSelectionEvent::wsGetSelectionEvent(JNIEnv *yourEnv, NativeBrowserControl *yourInitContext, jobject yourSelection) :
    nsActionEvent(),
    mEnv(yourEnv),
    mInitContext(yourInitContext),
    mSelection(yourSelection)
{
}

void *
wsGetSelectionEvent::handleEvent()
{
    void *result = nsnull;

    if (mEnv != nsnull && mInitContext != nsnull && mSelection != nsnull) {
        nsresult rv = nsnull;

        // Get the DOM window
        nsIDOMWindow *domWindow;
        rv = mInitContext->webBrowser->GetContentDOMWindow(&domWindow);
        if (NS_FAILED(rv) || domWindow == nsnull )  {
            return (void *) rv;
        }

        // Get the selection object of the DOM window
        nsISelection *selection;
        rv = domWindow->GetSelection(&selection);
        if (NS_FAILED(rv) || selection == nsnull)  {
            return (void *) rv;
        }

        // Get the range count
        PRInt32 rangeCount;
        rv = selection->GetRangeCount(&rangeCount);
        if (NS_FAILED(rv) || rangeCount == 0)  {
            return (void *) rv;
        }

        // Get the actual selection string
        PRUnichar *selectionStr;
        rv = selection->ToString(&selectionStr);
        if (NS_FAILED(rv))  {
            return (void *) rv;
        }

        jstring string =
            mEnv->NewString((jchar*)selectionStr, nsCRT::strlen(selectionStr));

        // Get the first range object of the selection object
        nsIDOMRange *range;
        rv = selection->GetRangeAt(0, &range);
        if (NS_FAILED(rv) || range == nsnull)  {
            return (void *) rv;
        }

        // Get the properties of the range object (startContainer,
        // startOffset, endContainer, endOffset)
        PRInt32 startOffset;
        PRInt32 endOffset;
        nsIDOMNode* startContainer;
        nsIDOMNode* endContainer;

        // start container
        rv = range->GetStartContainer(&startContainer);
        if (NS_FAILED(rv))  {
            return (void *) rv;
        }

        // end container
        rv = range->GetEndContainer(&endContainer);
        if (NS_FAILED(rv))  {
            return (void *) rv;
        }

        // start offset
        rv = range->GetStartOffset(&startOffset);
        if (NS_FAILED(rv))  {
            return (void *) rv;
        }

        // end offset
        rv = range->GetEndOffset(&endOffset);
        if (NS_FAILED(rv))  {
            return (void *) rv;
        }

        // get a handle on to acutal (java) Node representing the start
        // and end containers
        jlong node1Long = nsnull;
        jlong node2Long = nsnull;

        nsCOMPtr<nsIDOMNode> node1Ptr(do_QueryInterface(startContainer));
        nsCOMPtr<nsIDOMNode> node2Ptr(do_QueryInterface(endContainer));

        if (nsnull == (node1Long = (jlong)node1Ptr.get())) {
            return result;
        }
        if (nsnull == (node2Long = (jlong)node2Ptr.get())) {
            return result;
        }

        jclass clazz = nsnull;
        jmethodID mid = nsnull;

        if (nsnull == (clazz = ::util_FindClass(mEnv,
                                                "org/mozilla/dom/DOMAccessor"))) {
            return result;
        }
        if (nsnull == (mid = mEnv->GetStaticMethodID(clazz, "getNodeByHandle",
                                                    "(J)Lorg/w3c/dom/Node;"))) {
            return result;
        }

        jobject node1 = (jobject) ((void *)::util_CallStaticObjectMethodlongArg(mEnv, clazz, mid, node1Long));
        jobject node2 = (jobject) ((void *)::util_CallStaticObjectMethodlongArg(mEnv, clazz, mid, node2Long));

        // prepare the (java) Selection object that is to be returned.
        if (nsnull == (clazz = ::util_FindClass(mEnv, "org/mozilla/webclient/Selection"))) {
            return result;
        }

        if (nsnull == (mid = mEnv->GetMethodID(clazz, "init",
                                              "(Ljava/lang/String;Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;II)V"))) {
            return result;
        }

        mEnv->CallVoidMethod(mSelection, mid,
                             string, node1, node2,
                             (jint)startOffset, (jint)endOffset);
    }

    return result;
}


wsHighlightSelectionEvent::wsHighlightSelectionEvent(JNIEnv *yourEnv, NativeBrowserControl *yourInitContext, jobject startContainer, jobject endContainer, PRInt32 startOffset, PRInt32 endOffset) :
    nsActionEvent(),
    mEnv(yourEnv),
    mInitContext(yourInitContext),
    mStartContainer(startContainer),
    mEndContainer(endContainer),
    mStartOffset(startOffset),
    mEndOffset(endOffset)
{
}

void *
wsHighlightSelectionEvent::handleEvent()
{
    void *result = nsnull;

     if (mEnv != nsnull && mInitContext != nsnull &&
         mStartContainer != nsnull && mEndContainer != nsnull &&
         mStartOffset > -1 && mEndOffset > -1)
     {
        nsresult rv = nsnull;

        // resolve ptrs to the nodes
        jclass nodeClass = mEnv->FindClass("org/mozilla/dom/NodeImpl");
        if (!nodeClass) {
            return result;
        }
        jfieldID nodePtrFID = mEnv->GetFieldID(nodeClass, "p_nsIDOMNode", "J");
        if (!nodePtrFID) {
            return result;
        }

        // get the nsIDOMNode representation of the start and end containers
        nsIDOMNode* node1 = (nsIDOMNode*)
            mEnv->GetLongField(mStartContainer, nodePtrFID);

        nsIDOMNode* node2 = (nsIDOMNode*)
            mEnv->GetLongField(mEndContainer, nodePtrFID);

        if (!node1 || !node2) {
            return result;
        }

        // Get the DOM window
        nsIDOMWindow* domWindow;
        rv = mInitContext->webBrowser->GetContentDOMWindow(&domWindow);
        if (NS_FAILED(rv) || domWindow == nsnull )  {
            return (void *) rv;
        }

        // Get the selection object of the DOM window
        nsISelection* selection;
        rv = domWindow->GetSelection(&selection);
        if (NS_FAILED(rv) || selection == nsnull) {
            return (void *) rv;
        }

        nsCOMPtr<nsIDOMDocumentRange> docRange(do_QueryInterface(mInitContext->currentDocument));
        if (docRange) {
            nsCOMPtr<nsIDOMRange> range;
            rv = docRange->CreateRange(getter_AddRefs(range));

            if (range) {
                rv = range->SetStart(node1, mStartOffset);
                if (NS_FAILED(rv))  {
                    return (void *) rv;
                }

                rv = range->SetEnd(node2, mEndOffset);
                if (NS_FAILED(rv))  {
                    return (void *) rv;
                }

                rv = selection->AddRange(range);
                if (NS_FAILED(rv))  {
                    return (void *) rv;
                }
            }
        }
     }

    return result;
}

wsClearAllSelectionEvent::wsClearAllSelectionEvent(NativeBrowserControl *yourInitContext) :
    nsActionEvent(),
    mInitContext(yourInitContext)
{
}

void *
wsClearAllSelectionEvent::handleEvent()
{
    void *result = nsnull;

    if (mInitContext != nsnull) {
        nsresult rv = nsnull;

        // Get the DOM window
        nsIDOMWindow* domWindow;
        rv = mInitContext->webBrowser->GetContentDOMWindow(&domWindow);
        if (NS_FAILED(rv) || domWindow == nsnull )  {
            return (void *) rv;
        }

        // Get the selection object of the DOM window
        nsISelection* selection;
        rv = domWindow->GetSelection(&selection);
        if (NS_FAILED(rv) || selection == nsnull) {
            return (void *) rv;
        }

        rv = selection->RemoveAllRanges();
        if (NS_FAILED(rv)) {
            return (void *) rv;
        }
    }

     return result;
}

wsFindEvent::wsFindEvent(NativeBrowserControl *yourInitContext) :
    nsActionEvent(),
    mInitContext(yourInitContext),
    mSearchString(nsnull),
    mForward(JNI_FALSE),
    mMatchCase(JNI_FALSE)
{
}

wsFindEvent::wsFindEvent(NativeBrowserControl *yourInitContext, jstring searchString,
                         jboolean forward, jboolean matchCase) :
    nsActionEvent(),
    mInitContext(yourInitContext),
    mSearchString(searchString),
    mForward(forward),
    mMatchCase(matchCase)
{
}

void *
wsFindEvent::handleEvent ()
{
    void *result = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (mInitContext) {
        //First get the nsIWebBrowserFind object
        nsCOMPtr<nsIWebBrowserFind> findComponent;
        nsCOMPtr<nsIInterfaceRequestor>
            findRequestor(do_GetInterface(mInitContext->webBrowser));

        rv = findRequestor->GetInterface(NS_GET_IID(nsIWebBrowserFind),
                                         getter_AddRefs(findComponent));

        if (NS_FAILED(rv) || nsnull == findComponent)  {
            return (void *) rv;
        }

        // If this a Find, not a FindNext, we must make the
        // nsIWebBrowserFind instance aware of the string to search.
        if (mSearchString) {

            PRUnichar * srchString = nsnull;

            srchString = (PRUnichar *) ::util_GetStringChars(env,
                                                             mSearchString);

            // Check if String is NULL
            if (nsnull == srchString) {
                return (void *) NS_ERROR_NULL_POINTER;
            }
            rv = findComponent->SetSearchString(srchString);
            if (NS_FAILED(rv))  {
                mInitContext->initFailCode = kFindComponentError;
                return (void *) rv;
            }
            ::util_ReleaseStringChars(env, mSearchString, srchString);
            ::util_DeleteGlobalRef(env, mSearchString);
            mSearchString = nsnull;

        }
        findComponent->SetFindBackwards(!mForward);
        findComponent->SetMatchCase(mMatchCase);


        PRBool found = PR_TRUE;
        rv = findComponent->FindNext(&found);
        result = (void *) rv;

    }
    return result;
}

/*
 * wsGetURLEvent
 */

wsGetURLEvent::wsGetURLEvent(NativeBrowserControl *yourInitContext) :
        nsActionEvent(),
    mInitContext(yourInitContext)
{
}


void *
wsGetURLEvent::handleEvent ()
{
    void *result = nsnull;
    if (mInitContext) {
        nsISHistory* mHistory;
        nsresult rv;
        PRInt32 currentIndex;
        char *currentURL = nsnull;


        rv = mInitContext->webNavigation->GetSessionHistory(&mHistory);
        if (NS_FAILED(rv)) {
            return (void *) rv;
        }

        rv = mHistory->GetIndex(&currentIndex);

        if (NS_FAILED(rv)) {
            return result;
        }

        nsIHistoryEntry * Entry;
        rv = mHistory->GetEntryAtIndex(currentIndex, PR_FALSE, &Entry);

        if (NS_FAILED(rv)) {
            return result;
        }

    nsIURI * URI;
    rv = Entry->GetURI(&URI);

    if (NS_FAILED(rv)) {
            return result;
        }

    nsCString urlSpecString;

    rv = URI->GetSpec(urlSpecString);
    if (NS_FAILED(rv)) {
        return result;
    }
    currentURL = ToNewCString(urlSpecString);

    result = (void *) currentURL;
    }
    return result;
} // handleEvent()


wsSelectAllEvent::wsSelectAllEvent(NativeBrowserControl *yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsSelectAllEvent::handleEvent ()
{
    void *result = nsnull;

    if (mInitContext) {
        nsIContentViewer* contentViewer;
        nsresult rv = nsnull;
        rv = mInitContext->docShell->GetContentViewer(&contentViewer);
        if (NS_FAILED(rv) || contentViewer==nsnull)  {
            mInitContext->initFailCode = kGetContentViewerError;
            return (void *) rv;
        }

        nsCOMPtr<nsIContentViewerEdit> contentViewerEdit(do_QueryInterface(contentViewer));

        rv = contentViewerEdit->SelectAll();
        result = (void *) rv;
    }
    return result;
}

/* PENDING(ashuk): remove this from here and in the motif directory
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
*/


wsGetDOMEvent::wsGetDOMEvent(JNIEnv *yourEnv, jclass clz,
                             jmethodID yourID, jlong yourDoc) :
    nsActionEvent(),
    mEnv(yourEnv),
    mClazz(clz),
    mID(yourID),
    mDoc(yourDoc)
{
}

void *
wsGetDOMEvent::handleEvent ()
{

    void * result = nsnull;
    if (mEnv != nsnull && mClazz != nsnull &&
        mID != nsnull && mDoc != nsnull)
        result = (void *) util_CallStaticObjectMethodlongArg(mEnv, mClazz, mID, mDoc);

    return result;
}

// Deal with the Print events. TODO: we need a print setup UI in Java
wsPrintEvent::wsPrintEvent(NativeBrowserControl *yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsPrintEvent::handleEvent ()
{
    void *result = nsnull;

    if (mInitContext) {
        nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mInitContext->webBrowser));
        nsresult rv = nsnull;
        if (print==nsnull)  {
            mInitContext->initFailCode = kGetContentViewerError;
            return (void *) rv;
        }

        nsCOMPtr<nsIPrintSettings> printSettings;
        rv = print->GetGlobalPrintSettings(getter_AddRefs(printSettings));
        if (NS_FAILED(rv))
            return (void *) rv;

        // XXX kyle: we have to disable the Print Progress dialog until we are able to show the java native dialog.
        printSettings->SetShowPrintProgress(PR_FALSE);
        rv = print->Print(printSettings, nsnull);

        result = (void *) rv;
    }

    return result;
}


wsPrintPreviewEvent::wsPrintPreviewEvent(NativeBrowserControl *yourInitContext, jboolean preview) :
        nsActionEvent(),
        mInitContext(yourInitContext), mInPreview(preview)
{
}

void *
wsPrintPreviewEvent::handleEvent ()
{
    void *result = nsnull;

    if (mInitContext) {
        nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(mInitContext->webBrowser));
        nsresult rv = nsnull;
        if (print==nsnull)  {
            mInitContext->initFailCode = kGetContentViewerError;
            return (void *) rv;
        }

        nsCOMPtr<nsIPrintSettings> printSettings;
        rv = print->GetGlobalPrintSettings(getter_AddRefs(printSettings));
        if (NS_FAILED(rv))
            return (void *) rv;

        // XXX kyle: we have to disable the Print Progress dialog by now because we are unable to show the java native dialog yet.
        printSettings->SetShowPrintProgress(PR_FALSE);
        if (mInPreview) {
            rv = print->PrintPreview(printSettings, nsnull, nsnull);
        }
        else {
            rv = print->ExitPrintPreview();
        }

        result = (void *) rv;
    }

    return result;
}
