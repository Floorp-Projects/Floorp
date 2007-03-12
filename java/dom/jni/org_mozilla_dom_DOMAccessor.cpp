
#include "prlog.h"
#include "javaDOMGlobals.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIServiceManager.h"
#include "nsCURILoader.h"

#include "nsIJavaDOM.h"
#include "org_mozilla_dom_DOMAccessor.h"

static NS_DEFINE_IID(kJavaDOMCID, NS_JAVADOM_CID);

/*
 * Class:     org_mozilla_dom_DOMAccessor
 * Method:    register
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMAccessor_nativeRegister
  (JNIEnv *env, jclass jthis)
{
  if (!JavaDOMGlobals::log) {
    JavaDOMGlobals::Initialize(env);
  }
  nsresult rv = NS_OK; 
  nsCOMPtr<nsIDocumentLoader> docLoaderService = 
    do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  nsCOMPtr<nsIWebProgress> webProgressService;
  
  if (NS_FAILED(rv) || !docLoaderService) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMAccessor::register: GetService(JavaDOM) failed: %x\n", 
	    rv));
  } else {
    nsCOMPtr<nsIWebProgressListener> javaDOM;
    javaDOM = do_GetService(kJavaDOMCID, &rv);
    if (NS_FAILED(rv) || !javaDOM) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	     ("DOMAccessor::register: GetService(JavaDOM) failed: %x\n", 
	      rv));
    } else {
      webProgressService = do_QueryInterface(docLoaderService, &rv);
      if (NS_FAILED(rv) || !webProgressService) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	       ("DOMAccessor::register: QueryInterface(nsIWebProgressService) failed: %x\n", 
		rv));
      } else {
	rv = webProgressService->AddProgressListener((nsIWebProgressListener*)javaDOM, nsIWebProgress::NOTIFY_ALL);
	if (NS_FAILED(rv)) {
	  PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
		 ("DOMAccessor::register: AddObserver(JavaDOM) failed x\n", 
		  rv));
	}
      }
    }
  }
}

/*
 * Class:     org_mozilla_dom_DOMAccessor
 * Method:    unregister
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMAccessor_nativeUnregister
  (JNIEnv *, jclass jthis)
{
  PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	 ("DOMAccessor::unregister: unregistering %x\n", jthis));

  nsresult rv = NS_OK;
  nsCOMPtr<nsIDocumentLoader> docLoaderService = 
    do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  nsCOMPtr<nsIWebProgress> webProgressService;
  
  if (NS_FAILED(rv) || !docLoaderService) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMAccessor::unregister: GetService(DocLoaderService) failed %x\n", 
	    rv));
  } else {
    nsCOMPtr<nsIWebProgressListener> javaDOM;
    javaDOM = do_GetService(kJavaDOMCID, &rv);
    if (NS_FAILED(rv) || !javaDOM) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	     ("DOMAccessor::unregister: GetService(JavaDOM) failed %x\n", 
	      rv));
    } else {
      webProgressService = do_QueryInterface(docLoaderService, &rv);
      if (NS_FAILED(rv) || !webProgressService) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	       ("DOMAccessor::unregister: QueryInterface(nsIWebProgressService) failed: %x\n", 
		rv));
      } else {
	rv = webProgressService->RemoveProgressListener((nsIWebProgressListener*)javaDOM);
	if (NS_FAILED(rv)) {
	  PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
		 ("DOMAccessor::unregister: RemoveObserver(JavaDOM) failed x\n", 
		  rv));
	}
      }
    }
  }
}

/*
 * Class:     org_mozilla_dom_DOMAccessor
 * Method:    createElement
 * Signature: (J)Lorg/w3c/dom/Element;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DOMAccessor_nativeGetNodeByHandle
  (JNIEnv *env, jclass jthis, jlong p)
{ 
  if (!JavaDOMGlobals::log) {
    JavaDOMGlobals::Initialize(env);
  }
  nsIDOMNode *node = (nsIDOMNode*)p;
  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

/*
 * Class:     org_mozilla_dom_DOMAccessor
 * Method:    doGC
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMAccessor_nativeDoGC
  (JNIEnv *, jclass)
{
  JavaDOMGlobals::TakeOutGarbage();
}

JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMAccessor_nativeInitialize
(JNIEnv *env, jclass)
{
  if (!JavaDOMGlobals::log) {
    JavaDOMGlobals::Initialize(env);
  }
  
}


