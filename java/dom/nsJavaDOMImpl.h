#ifndef __nsJavaDOMImpl_h__
#define __nsJavaDOMImpl_h__

#include <jni.h>
#include "nsIJavaDOM.h"

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
			       nsresult aStatus,
			       nsIDocumentLoaderObserver* aObserver);

  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, 
			    nsIChannel* channel, 
                            nsIContentViewer* aViewer);

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
  static JavaVM*	jvm;
  static JNIEnv*	env;

  static jclass		domAccessorClass;
  static jclass		documentClass;
  static jclass		listenerClass;
  static jclass		gcClass;

  static jobject	docListener;

  static jfieldID	documentPtrFID;

  static jmethodID	getInstanceMID;
  static jmethodID	startURLLoadMID;
  static jmethodID	endURLLoadMID;
  static jmethodID	progressURLLoadMID;
  static jmethodID	statusURLLoadMID;
  static jmethodID	startDocumentLoadMID;
  static jmethodID	endDocumentLoadMID;

  static jmethodID	gcMID;

  // cleanup after a JNI method invocation
  static PRBool Cleanup();

  nsIDOMDocument* GetDocument(nsIDocumentLoader* loader);  
  jobject CaffienateDOMDocument(nsIDOMDocument* domDoc);
};

#endif /* __nsJavaDOMImpl_h__ */
