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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsWalletService_h___
#define nsWalletService_h___

#include "nsIWalletService.h"
#include "nsIObserver.h"
#include "nsIFormSubmitObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsIPasswordSink.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIURI.h"

class nsWalletlibService : public nsIWalletService,
                           public nsIObserver,
                           public nsIFormSubmitObserver,
                           public nsIDocumentLoaderObserver,
                           public nsIPasswordSink,
                           public nsSupportsWeakReference {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWALLETSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOCUMENTLOADEROBSERVER
  NS_DECL_NSIPASSWORDSINK
  // NS_DECL_NSSUPPORTSWEAKREFERENCE

  nsWalletlibService();

  // NS_DECL_NSIFORMSUBMITOBSERVER
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI* actionURL);
  
protected:
  virtual ~nsWalletlibService();

private:
  void    Init();
};

////////////////////////////////////////////////////////////////////////////////

class nsSingleSignOnPrompt : public nsISingleSignOnPrompt
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPT
  NS_DECL_NSISINGLESIGNONPROMPT

  nsSingleSignOnPrompt() { NS_INIT_REFCNT(); }
  virtual ~nsSingleSignOnPrompt() {}

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  nsCOMPtr<nsIPrompt>   mPrompt;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsWalletService_h___ */
