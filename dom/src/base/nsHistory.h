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
#ifndef nsHistory_h___
#define nsHistory_h___

#include "nsIScriptObjectOwner.h"
#include "nsIDOMNSHistory.h"
#include "nsISupports.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsISHistory.h"

class nsIDocShell;

// Script "History" object
class HistoryImpl : public nsIDOMNSHistory
{
public:
  HistoryImpl(nsIDocShell* aDocShell);
  virtual ~HistoryImpl();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMHistory
  NS_DECL_NSIDOMHISTORY

  // nsIDOMNSHistory
  NS_DECL_NSIDOMNSHISTORY

  void SetDocShell(nsIDocShell *aDocShell);

protected:
  nsresult GetSessionHistoryFromDocShell(nsIDocShell * aDocShell,
                                         nsISHistory ** aReturn);

  nsIDocShell* mDocShell;
};

#endif /* nsHistory_h___ */
