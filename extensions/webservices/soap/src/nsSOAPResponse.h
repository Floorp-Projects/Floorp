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

#ifndef nsSOAPResponse_h__
#define nsSOAPResponse_h__

#include "nsISOAPResponse.h"
#include "nsISecurityCheckedComponent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsCOMPtr.h"

class nsSOAPResponse : public nsISOAPResponse,
                       public nsISecurityCheckedComponent
{
public:
  nsSOAPResponse(nsIDOMDocument* aEnvelopeDocument);
  virtual ~nsSOAPResponse();

  NS_DECL_ISUPPORTS

  // nsISOAPResponse
  NS_DECL_NSISOAPRESPONSE

  // nsISecurityCheckedComponent
  NS_DECL_NSISECURITYCHECKEDCOMPONENT

  NS_IMETHOD SetStatus(PRUint32 aStatus);

protected:
  nsCOMPtr<nsIDOMDocument> mEnvelopeDocument;
  nsCOMPtr<nsIDOMElement> mEnvelopeElement;
  nsCOMPtr<nsIDOMElement> mHeaderElement;
  nsCOMPtr<nsIDOMElement> mBodyElement;
  nsCOMPtr<nsIDOMElement> mResultElement;
  nsCOMPtr<nsIDOMElement> mFaultElement;
  PRUint32 mStatus;
};

#endif
