/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsScriptLoader_h__
#define __nsScriptLoader_h__

#include "nsCOMPtr.h"
#include "nsIScriptLoader.h"
#include "nsIScriptElement.h"
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
  PRBool InNonScriptingContainer(nsIScriptElement* aScriptElement);
  PRBool IsScriptEventHandler(nsIScriptElement* aScriptElement);
  nsresult FireErrorNotification(nsresult aResult,
                                 nsIScriptElement* aElement,
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
