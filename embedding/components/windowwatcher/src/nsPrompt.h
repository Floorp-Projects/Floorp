/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIPromptService.h"

class nsPrompt : public nsIPrompt,
                 public nsIAuthPrompt {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPT
  NS_DECL_NSIAUTHPROMPT

  nsPrompt(nsIDOMWindow *window);
  virtual ~nsPrompt() {}

  nsresult Init();

protected:
  nsCOMPtr<nsIDOMWindow>        mParent;
  nsCOMPtr<nsIPromptService>    mPromptService;
};

nsresult
NS_NewPrompter(nsIPrompt **result, nsIDOMWindow *aParent);

nsresult
NS_NewAuthPrompter(nsIAuthPrompt **result, nsIDOMWindow *aParent);
