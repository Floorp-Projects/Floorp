/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWalletService_h___
#define nsWalletService_h___

#include "nsIWalletService.h"
#include "nsIObserver.h"
#include "nsIFormSubmitObserver.h"
#include "nsWeakReference.h"
#include "nsIPasswordSink.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIURI.h"
#include "nsIWebProgressListener.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"

class nsWalletlibService : public nsIWalletService,
                           public nsIObserver,
                           public nsIFormSubmitObserver,
                           public nsIWebProgressListener,
                           public nsIPasswordSink,
                           public nsSupportsWeakReference {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWALLETSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIPASSWORDSINK
  // NS_DECL_NSSUPPORTSWEAKREFERENCE

  nsWalletlibService();
  nsresult Init();

  // NS_DECL_NSIFORMSUBMITOBSERVER
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI* actionURL, PRBool* cancelSubmit);

  // {Un}Register proc that do category {Un}Registration
  static NS_METHOD RegisterProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info);
  static NS_METHOD UnregisterProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const nsModuleComponentInfo *info);

  
protected:
  virtual ~nsWalletlibService();
};

////////////////////////////////////////////////////////////////////////////////

class nsSingleSignOnPrompt : public nsISingleSignOnPrompt
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHPROMPT
  NS_DECL_NSISINGLESIGNONPROMPT

  nsSingleSignOnPrompt() { NS_INIT_REFCNT(); }
  virtual ~nsSingleSignOnPrompt() {}
  
  nsresult Init();

protected:
  nsCOMPtr<nsIPrompt>   mPrompt;
  static PRBool         mgRegisteredObserver;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsWalletService_h___ */
