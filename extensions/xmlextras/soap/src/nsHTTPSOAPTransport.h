/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsHTTPSOAPTransport_h__
#define nsHTTPSOAPTransport_h__

#include "nsISOAPTransport.h"
#include "nsIXMLHttpRequest.h"
#include "nsIDOMEventListener.h"
#include "nsISOAPTransportListener.h"
#include "nsISOAPCallCompletion.h"
#include "nsISOAPCall.h"
#include "nsISOAPResponse.h"
#include "nsISOAPResponseListener.h"
#include "nsCOMPtr.h"

class nsHTTPSOAPTransport:public nsISOAPTransport {
public:
  nsHTTPSOAPTransport();
  virtual ~ nsHTTPSOAPTransport();

  NS_DECL_ISUPPORTS
      // nsISOAPTransport
NS_DECL_NSISOAPTRANSPORT};

class nsHTTPSSOAPTransport:public nsHTTPSOAPTransport {
public:
  nsHTTPSSOAPTransport();
  virtual ~ nsHTTPSSOAPTransport();

NS_DECL_ISUPPORTS};

class nsHTTPSOAPTransportCompletion:public nsIDOMEventListener,
    public nsISOAPCallCompletion {
public:
  nsHTTPSOAPTransportCompletion();
  nsHTTPSOAPTransportCompletion(nsISOAPCall * call,
                                nsISOAPResponse * response,
                                nsIXMLHttpRequest * request,
                                nsISOAPResponseListener * listener);
  virtual ~ nsHTTPSOAPTransportCompletion();

  NS_DECL_ISUPPORTS NS_DECL_NSISOAPCALLCOMPLETION
      // nsIDOMEventListener
   NS_IMETHOD HandleEvent(nsIDOMEvent * aEvent);

protected:
   nsCOMPtr < nsISOAPCall > mCall;
   nsCOMPtr < nsISOAPResponse > mResponse;
   nsCOMPtr < nsIXMLHttpRequest > mRequest;
   nsCOMPtr < nsISOAPResponseListener > mListener;
};

#define NS_HTTPSOAPTRANSPORT_CID                   \
 { /* d852ade0-5823-11d4-9a62-00104bdf5339 */      \
 0xd852ade0, 0x5823, 0x11d4,                       \
 {0x9a, 0x62, 0x00, 0x10, 0x4b, 0xdf, 0x53, 0x39} }
#define NS_HTTPSOAPTRANSPORT_CONTRACTID NS_SOAPTRANSPORT_CONTRACTID_PREFIX "http"

#define NS_HTTPSOAPTRANSPORTCOMPLETION_CID                   \
 { /* 9032e336-1dd2-11b2-99be-a44376d4d5b1 */      \
 0x9032e336, 0x1dd2, 0x11b2,                       \
 {0x99, 0xbe, 0xa4, 0x43, 0x76, 0xd4, 0xd5, 0xb1} }
#define NS_HTTPSOAPTRANSPORTCOMPLETION_CONTRACTID "@mozilla.org/xmlextras/soap/transport/completion;1?protocol=http"

#define NS_HTTPSSOAPTRANSPORT_CID                   \
 { /* aad00c5a-1dd1-11b2-9ee1-c9f6b898a7b8 */      \
 0xaad00c5a, 0x1dd1, 0x11b2,                       \
 {0x9e, 0xe1, 0xc9, 0xf6, 0xb8, 0x98, 0xa7, 0xb8} }
#define NS_HTTPSSOAPTRANSPORT_CONTRACTID NS_SOAPTRANSPORT_CONTRACTID_PREFIX "https"
#endif
