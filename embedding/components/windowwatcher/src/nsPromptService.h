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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsPromptService_h
#define __nsPromptService_h

// {A2112D6A-0E28-421f-B46A-25C0B308CBD0}
#define NS_PROMPTSERVICE_CID \
 {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
#define NS_PROMPTSERVICE_CONTRACTID \
 "@mozilla.org/embedcomp/prompt-service;1"

#include "nsCOMPtr.h"
#include "nsIPromptService.h"
#include "nsPIPromptService.h"
#include "nsIWindowWatcher.h"

class nsIDOMWindow;
class nsIDialogParamBlock;

class nsPromptService: public nsIPromptService,
                       public nsPIPromptService {

public:

  nsPromptService();
  virtual ~nsPromptService();

  nsresult Init();

  NS_DECL_NSIPROMPTSERVICE
  NS_DECL_NSPIPROMPTSERVICE
  NS_DECL_ISUPPORTS

private:
  nsresult GetLocaleString(const char *aKey, PRUnichar **aResult);


  nsCOMPtr<nsIWindowWatcher> mWatcher;
};

#endif

