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
 * Copyright (C) 1998 Netscape Communications Corporation. All
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
#include "nsCOMPtr.h"

class nsHTTPSOAPTransport : public nsISOAPTransport,
			    public nsIDOMEventListener
{
public:
  nsHTTPSOAPTransport();
  virtual ~nsHTTPSOAPTransport();

  NS_DECL_ISUPPORTS

  // nsISOAPTransport
  NS_DECL_NSISOAPTRANSPORT

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

protected:
  PRUint32 mStatus;
  nsCOMPtr<nsIXMLHttpRequest> mRequest;
  nsCOMPtr<nsISOAPTransportListener> mListener;
};

#define NS_HTTPSOAPTRANSPORT_CID                   \
 { /* d852ade0-5823-11d4-9a62-00104bdf5339 */      \
 0xd852ade0, 0x5823, 0x11d4,                       \
 {0x9a, 0x62, 0x00, 0x10, 0x4b, 0xdf, 0x53, 0x39} }
#define NS_HTTPSOAPTRANSPORT_CONTRACTID NS_SOAPTRANSPORT_CONTRACTID_PREFIX "http"

#endif
