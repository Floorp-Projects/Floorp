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
			       nsIRequest* request, 
			       nsresult aStatus);

  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, 
			    nsIRequest* channel); 

  NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader,
			       nsIRequest* request, 
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax);

  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, 
			     nsIRequest* request, 
			     nsString& aMsg);

  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, 
			  nsIRequest* request, 
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

  static jmethodID	startURLLoadMID;
  static jmethodID	endURLLoadMID;
  static jmethodID	progressURLLoadMID;
  static jmethodID	statusURLLoadMID;
  static jmethodID	startDocumentLoadMID;
  static jmethodID	endDocumentLoadMID;

  static JNIEnv* GetJNIEnv(void);
  static void StartJVM(void);
  static PRBool Init(JNIEnv**);
  // cleanup after a JNI method invocation
  static PRBool Cleanup(JNIEnv* env);
  nsIDOMDocument* GetDocument(nsIDocumentLoader* loader);  
};

#endif /* __nsJavaDOMImpl_h__ */
