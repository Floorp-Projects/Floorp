/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

#ifndef __nsIJavaDOM_h__
#define __nsIJavaDOM_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIDocumentLoaderObserver.h"
#include "jni.h"

class nsIURI;

/* {9cc0ca50-0e31-11d3-8a61-0004ac56c4a5} */
#define NS_IJAVADOM_IID_STR "9cc0ca50-0e31-11d3-8a61-0004ac56c4a5"
#define NS_IJAVADOM_IID \
  {0x9cc0ca50, 0x0e31, 0x11d3, \
    { 0x8a, 0x61, 0x00, 0x04, 0xac, 0x56, 0xc4, 0xa5 }}

class nsIJavaDOM : public nsIDocumentLoaderObserver {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJAVADOM_IID)

  /* nsIDocumentLoaderObserver methods */
  NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, 
				 nsIURI* aURL, 
				 const char* aCommand) = 0;

  NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			       nsIChannel* channel, 
#else
			       nsIURI* aURL, 
#endif
			       PRInt32 aStatus,
			       nsIDocumentLoaderObserver* aObserver) = 0;

  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			    nsIChannel* channel, 
#else
			    nsIURI* aURL, 
			    const char* aContentType, 
#endif
                            nsIContentViewer* aViewer) = 0;  

  NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader,
#ifdef NECKO
			       nsIChannel* channel, 
#else
			       nsIURI* aURL, 
#endif
			       PRUint32 aProgress, 
                               PRUint32 aProgressMax) = 0;

  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			     nsIChannel* channel, 
#else
			     nsIURI* aURL, 
#endif
			     nsString& aMsg) = 0;

  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, 
#ifdef NECKO
			  nsIChannel* channel, 
#else
			  nsIURI* aURL, 
#endif
			  PRInt32 aStatus) = 0;

  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
#ifdef NECKO
				      nsIChannel* channel, 
#else
				      nsIURI* aURL, 
#endif
				      const char *aContentType,
				      const char *aCommand) = 0;
};

#endif /* __nsIJavaDOM_h__ */
