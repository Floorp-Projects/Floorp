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

  NS_DECL_NSIWEBPROGRESSLISTENER

  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
				      nsIChannel* channel, 
				      const char *aContentType,
				      const char *aCommand);

 protected:

 /**

  * Called from our nsIWebProgressListener.OnStateChanged()
  
  */ 
  
  NS_IMETHOD doStartDocumentLoad(const PRUnichar *documentName);
  NS_IMETHOD doStartUrlLoad(const PRUnichar *documentName);
  NS_IMETHOD doEndDocumentLoad(nsIWebProgress *aWebProgress, 
			       nsIRequest *aRequest, PRUint32 aStatus);

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
