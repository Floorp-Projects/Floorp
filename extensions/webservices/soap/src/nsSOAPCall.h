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

#ifndef nsSOAPCall_h__
#define nsSOAPCall_h__

#include "nsString.h"
#include "nsSOAPMessage.h"
#include "nsISOAPCall.h"
#include "nsISecurityCheckedComponent.h"
#include "nsISOAPTransport.h"
#include "nsISOAPResponseListener.h"
#include "nsCOMPtr.h"

class nsSOAPCall : public nsSOAPMessage,
                   public nsISOAPCall
{
public:
  nsSOAPCall();
  virtual ~nsSOAPCall();

  NS_DECL_ISUPPORTS

  // nsISOAPCall
  NS_FORWARD_NSISOAPMESSAGE(nsSOAPMessage::)

  // nsISOAPCall
  NS_DECL_NSISOAPCALL

  // nsISecurityCheckedComponent
  NS_DECL_NSISECURITYCHECKEDCOMPONENT

protected:

  nsString mTransportURI;
  nsresult GetTransport(nsISOAPTransport** aTransport);
};

#endif
