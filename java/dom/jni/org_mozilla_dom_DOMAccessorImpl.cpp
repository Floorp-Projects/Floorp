#include "org_mozilla_dom_DOMAccessorImpl.h"

#include "prlog.h"
#include "javaDOMGlobals.h"
#include "nsIDocumentLoader.h"
#include "nsIServiceManager.h"

#include "nsIJavaDOM.h"
#include "nsJavaDOMCID.h"

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);

static NS_DEFINE_IID(kJavaDOMFactoryCID, NS_JAVADOMFACTORY_CID);
static NS_DEFINE_IID(kIJavaDOMIID, NS_IJAVADOM_IID);

/*
 * Class:     org_mozilla_dom_DOMAccessorImpl
 * Method:    register
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMAccessorImpl_register
  (JNIEnv *, jclass jthis)
{
  PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	 ("DOMAccessor::register: registering %x\n", jthis));

  nsIDocumentLoader* docLoaderService = nsnull;
  nsISupports* javaDOM = nsnull;

  nsresult rv = nsServiceManager::GetService(kJavaDOMFactoryCID,
					     kIJavaDOMIID,
					     (nsISupports**) &javaDOM);
  if (NS_FAILED(rv) || !javaDOM) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMAccessor::register: GetService(JavaDOM) failed: %x\n", 
	    rv));
  } else {
    rv = docLoaderService->AddObserver((nsIDocumentLoaderObserver*)javaDOM);
    if (NS_FAILED(rv)) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	     ("DOMAccessor::register: AddObserver(JavaDOM) failed x\n", 
	      rv));
    }
  }
  nsServiceManager::ReleaseService(kDocLoaderServiceCID, docLoaderService);
}

/*
 * Class:     org_mozilla_dom_DOMAccessorImpl
 * Method:    unregister
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMAccessorImpl_unregister
  (JNIEnv *, jclass jthis)
{
  PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	 ("DOMAccessor::unregister: unregistering %x\n", jthis));

  nsIDocumentLoader* docLoaderService = nsnull;
  nsISupports* javaDOM = nsnull;

  nsresult rv = nsServiceManager::GetService(kJavaDOMFactoryCID,
					     kIJavaDOMIID,
					     (nsISupports**) &javaDOM);
  if (NS_FAILED(rv) || !javaDOM) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMAccessor::unregister: GetService(JavaDOM) failed %x\n", 
	    rv));
  } else {
    rv = docLoaderService->RemoveObserver((nsIDocumentLoaderObserver*)javaDOM);
    if (NS_FAILED(rv)) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	     ("DOMAccessor::unregister: RemoveObserver(JavaDOM) failed x\n", 
	      rv));
    }
  }
  nsServiceManager::ReleaseService(kDocLoaderServiceCID, docLoaderService);
}
