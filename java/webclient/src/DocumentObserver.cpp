#include "nsString.h"
#include "DocumentObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsISupportsUtils.h"
#include<stdio.h>
#include "jni_util.h"
#include <jni.h>

void fireDocumentLoadEvent(char *);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID, NS_IDOCUMENT_LOADER_OBSERVER_IID);


NS_IMPL_ADDREF(DocumentObserver);
NS_IMPL_RELEASE(DocumentObserver);  

jobject peer;
JavaVM *vm = NULL;

DocumentObserver::DocumentObserver(){
}

DocumentObserver::DocumentObserver(JNIEnv *env, jobject obj,
						 jint webShellPtr,
						 jobject doclistener) {
  env->GetJavaVM(&vm);  // save this vm reference away for the callback!
  peer = env->NewGlobalRef(doclistener);
}
 
NS_IMETHODIMP DocumentObserver::QueryInterface(REFNSIID aIID, void** aInstance)
{
  if (NULL == aInstance)
    return NS_ERROR_NULL_POINTER;

  *aInstance = NULL;

 
  if (aIID.Equals(kIDocumentLoaderObserverIID)) {
    *aInstance = (void*) ((nsIDocumentLoaderObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  

  return NS_NOINTERFACE;
}

  /* nsIDocumentLoaderObserver methods */
  NS_IMETHODIMP DocumentObserver::OnStartDocumentLoad(nsIDocumentLoader* loader, 
				 nsIURI* aURL, 
				 const char* aCommand) 
{
  printf("DocumentObserver.cpp: OnStartDocumentLoad\n");
    fireDocumentLoadEvent("startdocumentload");

  return NS_OK;
}

  NS_IMETHODIMP DocumentObserver::OnEndDocumentLoad(nsIDocumentLoader* loader, 
			       nsIChannel* channel, 
			       nsresult aStatus) {
    printf("!!DocumentObserver.cpp: OnEndDocumentLoad\n");
    fireDocumentLoadEvent("enddocumentload");
   
    return NS_OK;
  }

void fireDocumentLoadEvent(char *eventname) {
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(vm, JNI_VERSION_1_2);

    if (env == NULL) {
      return;
    }

    jclass clazz = env->GetObjectClass(peer);
    jmethodID mid = env->GetMethodID(clazz, "docEventPerformed", "(Ljava/lang/String;)V");
    jstring testmsg = env->NewStringUTF(eventname);
    if ( mid != NULL) {
      env->CallVoidMethod(peer, mid, testmsg);
    } else {
      printf("cannot call the Java Method!\n");
    }
}

  NS_IMETHODIMP DocumentObserver::OnStartURLLoad(nsIDocumentLoader* loader, 
			    nsIChannel* channel) {
    printf("!DocumentObserver: OnStartURLLoad\n");
    fireDocumentLoadEvent("startURLload");

    return NS_OK;}  

  NS_IMETHODIMP DocumentObserver::OnProgressURLLoad(nsIDocumentLoader* loader,
#ifdef NECKO
			       nsIChannel* channel, 
#else
			       nsIURI* aURL, 
#endif
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax) {
    printf("!DocumentObserver: OnProgressURLLoad\n");
    fireDocumentLoadEvent("progressURLload");

    return NS_OK;}

  NS_IMETHODIMP DocumentObserver::OnStatusURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			     nsIChannel* channel, 
#else
			     nsIURI* aURL, 
#endif
			     nsString& aMsg) {
    printf("!DocumentObserver: OnStatusURLLoad\n");
    fireDocumentLoadEvent("statusURLload");

    return NS_OK;}
 NS_IMETHODIMP DocumentObserver::OnEndURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			  nsIChannel* channel, 
			  nsresult aStatus) {
       printf("!DocumentObserver: OnEndURLLoad\n");
    fireDocumentLoadEvent("onendURLload");

       return NS_OK;}
#else
			  nsIURI* aURL, 
			  PRInt32 aStatus) {return NS_OK;}
#endif

  NS_IMETHODIMP DocumentObserver::HandleUnknownContentType(nsIDocumentLoader* loader,
#ifdef NECKO
				      nsIChannel* channel, 
#else
				      nsIURI* aURL, 
#endif
				      const char *aContentType,
				      const char *aCommand) {
    printf("!DocumentObserver: HandleUnknownContentType\n");
fireDocumentLoadEvent("handleunknown content");

return NS_OK;}



