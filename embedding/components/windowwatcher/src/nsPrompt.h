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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIPromptService.h"
#include "nsIPromptService2.h"

class nsPrompt : public nsIPrompt,
                 public nsIAuthPrompt,
                 public nsIAuthPrompt2 {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPT
  NS_DECL_NSIAUTHPROMPT
  NS_DECL_NSIAUTHPROMPT2

  nsPrompt(nsIDOMWindow *window);
  virtual ~nsPrompt() {}

  nsresult Init();

  /**
   * This helper method can be used to implement nsIAuthPrompt2's
   * PromptPassword function using nsIPromptService (as opposed to
   * nsIPromptService2).
   */
  static nsresult PromptPasswordAdapter(nsIPromptService* aService,
                                        nsIDOMWindow* aParent,
                                        nsIChannel* aChannel,
                                        PRUint32 aLevel,
                                        nsIAuthInformation* aAuthInfo,
                                        const PRUnichar* aCheckLabel,
                                        PRBool* aCheckValue,
                                        PRBool* retval);

protected:
  nsCOMPtr<nsIDOMWindow>        mParent;
  nsCOMPtr<nsIPromptService>    mPromptService;
  // This is the new prompt service version. May be null.
  nsCOMPtr<nsIPromptService2>   mPromptService2;
};

/**
 * A class that wraps an nsIAuthPrompt so that it can be used as an
 * nsIAuthPrompt2.
 */
class AuthPromptWrapper : public nsIAuthPrompt2
{
  public:
    AuthPromptWrapper(nsIAuthPrompt* aAuthPrompt) :
      mAuthPrompt(aAuthPrompt) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHPROMPT2

  private:
    ~AuthPromptWrapper() {}

    nsCOMPtr<nsIAuthPrompt> mAuthPrompt;
};

nsresult
NS_NewPrompter(nsIPrompt **result, nsIDOMWindow *aParent);

nsresult
NS_NewAuthPrompter(nsIAuthPrompt **result, nsIDOMWindow *aParent);

nsresult
NS_NewAuthPrompter2(nsIAuthPrompt2 **result, nsIDOMWindow *aParent);
