#ifndef __nsJavaDOMImpl_h__
#define __nsJavaDOMImpl_h__

#include "jni.h"
#include "nsIJavaDOM.h"
#ifdef JAVA_DOM_OJI_ENABLE
#include "nsJVMManager.h"
#include "JavaDOMSecurityContext.h"
#endif 

class nsIURI;
class nsIDOMDocument;
class nsIDocumentLoader;

class nsJavaDOMImpl : public nsIJavaDOM {
  NS_DECL_ISUPPORTS

 public:
  nsJavaDOMImpl();
  virtual ~nsJavaDOMImpl();

  /* nsIDocumentLoaderObserver methods */
  NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, 
				 nsIURI* aURL, 
				 const char* aCommand);

  NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, 
			       nsIChannel* channel, 
			       nsresult aStatus);

  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, 
			    nsIChannel* channel); 

  NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader,
			       nsIChannel* channel, 
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax);

  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, 
			     nsIChannel* channel, 
			     nsString& aMsg);

  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, 
			  nsIChannel* channel, 
			  nsresult aStatus);

  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
				      nsIChannel* channel, 
				      const char *aContentType,
				      const char *aCommand);

 private:
#ifdef JAVA_DOM_OJI_ENABLE
  static nsJVMManager* jvmManager;
  static JavaDOMSecurityContext* securityContext;
#else
  static JavaVM*        jvm;
#endif

  static jclass		domAccessorClass;
  static jclass		documentClass;
  static jclass		listenerClass;
  static jclass		gcClass;

  static jobject	docListener;

  static jfieldID	documentPtrFID;
  static jmethodID      documentInitID;

  static jmethodID	getInstanceMID;
  static jmethodID	startURLLoadMID;
  static jmethodID	endURLLoadMID;
  static jmethodID	progressURLLoadMID;
  static jmethodID	statusURLLoadMID;
  static jmethodID	startDocumentLoadMID;
  static jmethodID	endDocumentLoadMID;

  static jmethodID	gcMID;

  // cleanup after a JNI method invocation
  static PRBool Cleanup(JNIEnv* env);
  static JNIEnv* GetJNIEnv(void);
  static void StartJVM(void);
  nsIDOMDocument* GetDocument(nsIDocumentLoader* loader);  
  jobject CaffienateDOMDocument(nsIDOMDocument* domDoc);
};

#endif /* __nsJavaDOMImpl_h__ */
