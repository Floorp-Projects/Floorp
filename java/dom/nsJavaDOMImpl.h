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
#ifdef NECKO
			       nsIChannel* channel, 
			       nsresult aStatus,
#else
			       nsIURI* aURL, 
			       PRInt32 aStatus,
#endif
			       nsIDocumentLoaderObserver* aObserver);

  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			    nsIChannel* channel, 
#else
			    nsIURI* aURL, 
			    const char* aContentType, 
#endif
                            nsIContentViewer* aViewer);

  NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader,
#ifdef NECKO
			       nsIChannel* channel, 
#else
			       nsIURI* aURL, 
#endif
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax);

  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			     nsIChannel* channel, 
#else
			     nsIURI* aURL, 
#endif
			     nsString& aMsg);

  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			  nsIChannel* channel, 
			  nsresult aStatus);
#else
			  nsIURI* aURL, 
			  PRInt32 aStatus);
#endif

  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
#ifdef NECKO
				      nsIChannel* channel, 
#else
				      nsIURI* aURL, 
#endif
				      const char *aContentType,
				      const char *aCommand);

 private:
  static JavaVM*	jvm;
  static JNIEnv*	env;

  static jclass		factoryClass;
  static jclass		documentClass;
  static jclass		listenerClass;
  static jclass		gcClass;

  static jobject	factory;
  static jobject	docListener;

  static jfieldID	documentPtrFID;

  static jmethodID	getListenerMID;
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
