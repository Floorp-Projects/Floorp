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
 *               Ann Sunhachawee
 */

#ifndef DocumentLoaderObserverImpl_h
#define DocumentLoaderObserverImpl_h

#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIDocumentLoaderObserver.h"

#include "jni_util.h"

class nsIURI;

/**

 * This class is the shim between the mozilla listener event system for
 * document load events and the java DocumentLoadListener interface.
 * For each of the On* methods, we call the appropriate method in java.
 * See the implementation of OnEndDocumentLoad for an example.

 */

class DocumentLoaderObserverImpl : public nsIDocumentLoaderObserver {
  NS_DECL_ISUPPORTS
public: 

typedef enum {
  START_DOCUMENT_LOAD_EVENT_MASK = 0,
  END_DOCUMENT_LOAD_EVENT_MASK,
  START_URL_LOAD_EVENT_MASK,
  END_URL_LOAD_EVENT_MASK,
  PROGRESS_URL_LOAD_EVENT_MASK,
  STATUS_URL_LOAD_EVENT_MASK,
  UNKNOWN_CONTENT_EVENT_MASK,
  FETCH_INTERRUPT_EVENT_MASK,
  NUMBER_OF_MASK_NAMES
} EVENT_MASK_NAMES;


#ifdef XP_UNIX
static jlong maskValues [NUMBER_OF_MASK_NAMES];
#else
static jlong maskValues [DocumentLoaderObserverImpl::EVENT_MASK_NAMES::NUMBER_OF_MASK_NAMES];
#endif

static char *maskNames [];

    DocumentLoaderObserverImpl(JNIEnv *yourJNIEnv, 
                               WebShellInitContext *yourInitContext,
                               jobject yourTarget);
    
    DocumentLoaderObserverImpl();

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

protected:

  JNIEnv *mJNIEnv;
  WebShellInitContext *mInitContext;
  jobject mTarget;
  
};

#endif // DocumentLoaderObserverImpl_h
