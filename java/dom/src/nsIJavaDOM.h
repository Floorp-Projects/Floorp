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

#ifndef __nsIJavaDOM_h__
#define __nsIJavaDOM_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIWebProgressListener.h"
#include "jni.h"

class nsIURI;

/* {9cc0ca50-0e31-11d3-8a61-0004ac56c4a5} */
#define NS_IJAVADOM_IID_STR "9cc0ca50-0e31-11d3-8a61-0004ac56c4a5"
#define NS_IJAVADOM_IID \
  {0x9cc0ca50, 0x0e31, 0x11d3, \
    { 0x8a, 0x61, 0x00, 0x04, 0xac, 0x56, 0xc4, 0xa5 }}
#define NS_JAVADOM_CID \
  {0xd6b2e820, 0x9113, 0x11d3, \
    { 0x9c, 0x11, 0x0, 0x10, 0x5a , 0xe3, 0x80 , 0x1e }}

class nsIJavaDOM : public nsIWebProgressListener {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJAVADOM_IID)

  NS_IMETHOD OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, 
			   PRUint32 aStateFlags, PRUint32 aStatus) = 0;

  NS_IMETHOD OnProgressChange(nsIWebProgress *aWebProgress, 
			      nsIRequest *aRequest, PRInt32 aCurSelfProgress, 
			      PRInt32 aMaxSelfProgress, 
			      PRInt32 aCurTotalProgress, 
			      PRInt32 aMaxTotalProgress) = 0;

  NS_IMETHOD OnLocationChange(nsIWebProgress *aWebProgress, 
			      nsIRequest *aRequest, nsIURI *location) = 0;

  NS_IMETHOD OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest,
			    nsresult aStatus, const PRUnichar *aMessage) = 0;

  NS_IMETHOD OnSecurityChange(nsIWebProgress *aWebProgress, 
			      nsIRequest *aRequest, PRUint32 state) = 0;


  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
				      nsIChannel* channel, 
				      const char *aContentType,
				      const char *aCommand) = 0;
};
#endif /* __nsIJavaDOM_h__ */
