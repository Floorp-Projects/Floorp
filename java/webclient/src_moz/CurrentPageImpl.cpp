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
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 */

/*
 * CurrentPageImpl.cpp
 */

#include "CurrentPageImpl.h"

#include "ns_util.h"
#include "rdf_util.h"
#include "nsActions.h"

#include "nsLayoutCID.h"
#include "nsCRT.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIFindComponent.h"
#include "nsISearchContext.h"
#include "nsIDocShell.h"
#include "nsIDOMSelection.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIContentViewer.h"
#include "nsIServiceManager.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerEdit.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIInterfaceRequestor.h"

static NS_DEFINE_CID(kCDOMRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);


JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    nsIContentViewer* contentViewer ;
    nsresult rv = nsnull;
    rv = initContext->docShell->GetContentViewer(&contentViewer);
    if (NS_FAILED(rv) || contentViewer==nsnull )  {
        initContext->initFailCode = kGetContentViewerError;
        ::util_ThrowExceptionToJava(env, "Exception: cant get ContentViewer from DocShell");
        return;
    }
    
    nsCOMPtr<nsIContentViewerEdit> contentViewerEdit(do_QueryInterface(contentViewer));
    rv = contentViewerEdit->CopySelection();
 
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindInPage
 * Signature: (Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeFindInPage
(JNIEnv *env, jobject obj, jint webShellPtr, jstring searchString, jboolean forward, jboolean matchCase)
{

  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

  
  //First get the FindComponent object
  nsresult rv;
  
  NS_WITH_SERVICE(nsIFindComponent, findComponent, NS_IFINDCOMPONENT_PROGID, &rv);
  if (NS_FAILED(rv) || nsnull == findComponent)  {
        initContext->initFailCode = kFindComponentError;
        ::util_ThrowExceptionToJava(env, "Exception: can't access FindComponent Service");
        return;
  }

  nsCOMPtr<nsIDOMWindow> domWindow;
  if (initContext->docShell != nsnull) {
      nsCOMPtr<nsIInterfaceRequestor> interfaceRequestor(do_QueryInterface(initContext->docShell));
      nsCOMPtr<nsIURI> url;
      rv = initContext->docShell->GetCurrentURI(getter_AddRefs(url));
      if (NS_FAILED(rv) || nsnull == url)  {
          ::util_ThrowExceptionToJava(env, "Exception: NULL URL passed to Find call");
          return;
      } 

      if (interfaceRequestor != nsnull) {
          rv = interfaceRequestor->GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(domWindow));
          if (NS_FAILED(rv) || nsnull == domWindow)  {
              initContext->initFailCode = kGetDOMWindowError;
              ::util_ThrowExceptionToJava(env, "Exception: cant get DOMWindow from DocShell");
              return;
          }
      }
      else
          {
              initContext->initFailCode = kFindComponentError;
              ::util_ThrowExceptionToJava(env, "Exception: cant get InterfaceRequestor from DocShell");
              return;
          }
  }
  else
      {
          initContext->initFailCode = kFindComponentError;
          ::util_ThrowExceptionToJava(env, "Exception: DocShell is not initialized");
          return;
      }

  nsCOMPtr<nsISupports> searchContext;
  rv = findComponent->CreateContext(domWindow, nsnull, getter_AddRefs(searchContext));
  if (NS_FAILED(rv))  {
        initContext->initFailCode = kSearchContextError;
        ::util_ThrowExceptionToJava(env, "Exception: can't create SearchContext for Find");
        return;
  }

  nsCOMPtr<nsISearchContext> srchcontext;
  rv = searchContext->QueryInterface(NS_GET_IID(nsISearchContext), getter_AddRefs(srchcontext));
  if (NS_FAILED(rv))  {
        initContext->initFailCode = kSearchContextError;
        ::util_ThrowExceptionToJava(env, "Exception: can't create SearchContext for Find");
        return;
  }

  PRUnichar * aString;
  srchcontext->GetSearchString(& aString);

  PRUnichar * srchString = (PRUnichar *) ::util_GetStringChars(env, searchString);

  // Check if String is NULL
  if (nsnull == srchString) {
      ::util_ThrowExceptionToJava(env, "Exception: NULL String passed to Find call");
      return;
  }

  srchcontext->SetSearchString(srchString);
  srchcontext->SetSearchBackwards(!forward);
  srchcontext->SetCaseSensitive(matchCase);

  // Pass searchContext to findComponent for the actual find call
  PRBool found = PR_TRUE;
  findComponent->FindNext(srchcontext, &found);

  // Save in initContext struct for future findNextInPage calls
  initContext->searchContext = srchcontext;

}



/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindNextInPage
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeFindNextInPage
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean forward)
{

  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
  //First get the FindComponent object
  nsresult rv;
  
  NS_WITH_SERVICE(nsIFindComponent, findComponent, NS_IFINDCOMPONENT_PROGID, &rv);
  if (NS_FAILED(rv))  {
        initContext->initFailCode = kFindComponentError;
        ::util_ThrowExceptionToJava(env, "Exception: can't access FindComponent Service");
        return;
  }

  // Get the searchContext from the initContext struct
  nsCOMPtr<nsISearchContext> searchContext = initContext->searchContext;

  if (nsnull == searchContext) {
     initContext->initFailCode = kSearchContextError;
        ::util_ThrowExceptionToJava(env, "Exception: NULL SearchContext received for FindNext");
        return;
  }

  // Set the forward flag as per input parameter
  searchContext->SetSearchBackwards(!forward);

  // Pass searchContext to findComponent for the actual find call
  PRBool found = PR_TRUE;
  findComponent->FindNext(searchContext, &found);

}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetCurrentURL
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetCurrentURL
(JNIEnv *env, jobject obj, jint webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	char	*	charResult = nsnull;
	jstring		urlString = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetURL");
		return nsnull;
	}

	if (initContext->initComplete) {
	  //        nsISessionHistory *yourHistory;
	nsISHistory* yourHistory;
        nsresult rv;
        
        rv = initContext->webNavigation->GetSessionHistory(&yourHistory);
        
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Exception: can't get SessionHistory from webNavigation");
            return urlString;
        }

		wsGetURLEvent	* actionEvent = new wsGetURLEvent(yourHistory);
        PLEvent	   	  	* event       = (PLEvent*) *actionEvent;

		charResult = (char *) ::util_PostSynchronousEvent(initContext, event);

		if (charResult != nsnull) {
			urlString = ::util_NewStringUTF(env, (const char *) charResult);
		}
		else {
			::util_ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: GetURL() returned NULL");
			return nsnull;
		}

        nsCRT::free(charResult);
	}

	return urlString;
}

JNIEXPORT jobject JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetDOM
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    jobject result = nsnull;
    jlong documentLong = nsnull;
    jclass clazz = nsnull;
    jmethodID mid = nsnull;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetDOM");
		return nsnull;
	}
    if (nsnull == initContext->currentDocument ||
        nsnull == (documentLong = (jlong) initContext->currentDocument.get())){
        return nsnull;
    }
    
	if (nsnull == (clazz = ::util_FindClass(env, 
                                            "org/mozilla/dom/DOMAccessor"))) {
		::util_ThrowExceptionToJava(env, "Exception: Can't get DOMAccessor class");
		return nsnull;
	}
    if (nsnull == (mid = env->GetStaticMethodID(clazz, "getNodeByHandle",
                                                "(J)Lorg/w3c/dom/Node;"))) {
		::util_ThrowExceptionToJava(env, "Exception: Can't get DOM Node.");
		return nsnull;
	}
    result = env->CallStaticObjectMethod(clazz, mid, documentLong);
    
    return result;
}


/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSource
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetSource
(JNIEnv * env, jobject jobj)
{
    jstring result = nsnull;
    
    return result;
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSourceBytes
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetSourceBytes
(JNIEnv * env, jobject jobj, jint webShellPtr, jboolean viewMode)
{


  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

  if (initContext->initComplete) {
      wsViewSourceEvent * actionEvent = 
          new wsViewSourceEvent(initContext->docShell, ((JNI_TRUE == viewMode)? PR_TRUE : PR_FALSE));
      PLEvent	   	* event       = (PLEvent*) *actionEvent;

      ::util_PostSynchronousEvent(initContext, event);
  }

    jbyteArray result = nsnull;
    return result;
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeResetFind
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeResetFind
(JNIEnv * env, jobject obj, jint webShellPtr)
{
  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
  initContext->searchContext = nsnull;
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeSelectAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeSelectAll
(JNIEnv * env, jobject obj, jint webShellPtr)
{
        WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
        nsIContentViewer* contentViewer ;
        nsresult rv = nsnull;
        rv = initContext->docShell->GetContentViewer(&contentViewer);
        if (NS_FAILED(rv) || contentViewer==nsnull)  {
            initContext->initFailCode = kGetContentViewerError;
            ::util_ThrowExceptionToJava(env, "Exception: cant get ContentViewer from DocShell");
            return;
        }

        nsCOMPtr<nsIContentViewerEdit> contentViewerEdit(do_QueryInterface(contentViewer));
        rv = contentViewerEdit->SelectAll();
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Exception: can't do SelectAll through contentViewerEdit");
        }
}
