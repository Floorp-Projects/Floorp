/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#ifndef __nsScriptLoader_h__
#define __nsScriptLoader_h__

#include "nsCOMPtr.h"
#include "nsIScriptLoader.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIURI.h"
#include "nsCOMArray.h"
#include "nsIDocument.h"
#include "nsIStreamLoader.h"

class nsScriptLoadRequest;

//////////////////////////////////////////////////////////////
// Script loader implementation
//////////////////////////////////////////////////////////////

class nsScriptLoader : public nsIScriptLoader,
                       public nsIStreamLoaderObserver
{
public:
  nsScriptLoader();
  virtual ~nsScriptLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADER
  NS_DECL_NSISTREAMLOADEROBSERVER

protected:
  PRBool InNonScriptingContainer(nsIDOMHTMLScriptElement* aScriptElement);
  PRBool IsScriptEventHandler(nsIDOMHTMLScriptElement* aScriptElement);
  nsresult FireErrorNotification(nsresult aResult,
                                 nsIDOMHTMLScriptElement* aElement,
                                 nsIScriptLoaderObserver* aObserver);
  nsresult ProcessRequest(nsScriptLoadRequest* aRequest);
  void FireScriptAvailable(nsresult aResult,
                           nsScriptLoadRequest* aRequest,
                           const nsAFlatString& aScript);
  void FireScriptEvaluated(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  nsresult EvaluateScript(nsScriptLoadRequest* aRequest,
                          const nsAFlatString& aScript);
  void ProcessPendingReqests();

  nsIDocument* mDocument;                   // [WEAK]
  nsCOMArray<nsIScriptLoaderObserver> mObservers;
  nsCOMArray<nsScriptLoadRequest> mPendingRequests;
  PRBool mEnabled;
};

#endif //__nsScriptLoader_h__
