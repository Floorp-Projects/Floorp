/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/


#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIDocumentLoaderObserver.h"
#include "jni.h"

class nsIURI;


class DocumentObserver : public nsIDocumentLoaderObserver {
  NS_DECL_ISUPPORTS
 public: 

  DocumentObserver::DocumentObserver(JNIEnv *env, jobject obj,
						 jint webShellPtr,
				      jobject doclistener);

  DocumentObserver::DocumentObserver();

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
  
};



