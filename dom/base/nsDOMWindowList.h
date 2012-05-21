/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMWindowList_h___
#define nsDOMWindowList_h___

#include "nsISupports.h"
#include "nsIDOMWindowCollection.h"
#include "nsString.h"

class nsIDocShellTreeNode;
class nsIDocShell;
class nsIDOMWindow;

class nsDOMWindowList : public nsIDOMWindowCollection
{
public:
  nsDOMWindowList(nsIDocShell *aDocShell);
  virtual ~nsDOMWindowList();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWINDOWCOLLECTION

  //local methods
  NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);

protected:
  nsIDocShellTreeNode* mDocShellNode; //Weak Reference
};

#endif // nsDOMWindowList_h___
