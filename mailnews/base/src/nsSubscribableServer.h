/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Seth Spitzer <sspitzer@netscape.com>
 */

#ifndef nsSubscribableServer_h__
#define nsSubscribableServer_h__

#include "nsISubscribableServer.h"
#include "nsCOMPtr.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsIMsgIncomingServer.h"

class nsSubscribableServer : public nsISubscribableServer
{
 public:
  nsSubscribableServer();
  virtual ~nsSubscribableServer();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISUBSCRIBABLESERVER
  
private:
  nsresult ConvertNameToUnichar(const char *inStr, PRUnichar **outStr);
  nsCOMPtr <nsISubscribeListener> mSubscribeListener;
  nsCOMPtr <nsIRDFDataSource> mSubscribeDatasource;
  nsCOMPtr <nsIRDFService> mRDFService;
  nsCOMPtr <nsIRDFResource> kNC_Name;
  nsCOMPtr <nsIRDFResource> kNC_Child;
  nsCOMPtr <nsIRDFResource> kNC_Subscribed;
  nsCOMPtr <nsIRDFLiteral> kTrueLiteral;
  nsCOMPtr <nsIRDFLiteral> kFalseLiteral; 
  nsCOMPtr <nsIMsgIncomingServer> mIncomingServer;
  char mDelimiter;
};

#endif // nsSubscribableServer_h__
