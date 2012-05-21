/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsHistory_h___
#define nsHistory_h___

#include "nsIDOMHistory.h"
#include "nsISupports.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsISHistory.h"
#include "nsIWeakReference.h"
#include "nsPIDOMWindow.h"

class nsIDocShell;

// Script "History" object
class nsHistory : public nsIDOMHistory
{
public:
  nsHistory(nsPIDOMWindow* aInnerWindow);
  virtual ~nsHistory();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMHistory
  NS_DECL_NSIDOMHISTORY

  nsIDocShell *GetDocShell() {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mInnerWindow));
    if (!win)
      return nsnull;
    return win->GetDocShell();
  }

  void GetWindow(nsPIDOMWindow **aWindow) {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mInnerWindow));
    *aWindow = win.forget().get();
  }

protected:
  nsresult GetSessionHistoryFromDocShell(nsIDocShell * aDocShell,
                                         nsISHistory ** aReturn);

  nsCOMPtr<nsIWeakReference> mInnerWindow;
};

#endif /* nsHistory_h___ */
