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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgWindow_h
#define _nsMsgWindow_h

#include "nsIMsgWindow.h"
#include "nsIMsgStatusFeedback.h"
#include "nsITransactionManager.h"
#include "nsIMessageView.h"
#include "nsIMsgFolder.h"
#include "nsIDocShell.h"
#include "nsIURIContentListener.h"
#include "nsIMimeMiscStatus.h"
#include "nsWeakReference.h"

#include "nsCOMPtr.h"

class nsMsgWindow : public nsIMsgWindow, public nsIURIContentListener {

public:

  NS_DECL_ISUPPORTS

  nsMsgWindow();
  virtual ~nsMsgWindow();
  nsresult Init();
  NS_DECL_NSIMSGWINDOW
  NS_DECL_NSIURICONTENTLISTENER

protected:
  nsCOMPtr<nsIMsgHeaderSink> mMsgHeaderSink;
  nsCOMPtr<nsIMsgStatusFeedback> mStatusFeedback;
  nsCOMPtr<nsITransactionManager> mTransactionManager;
  nsCOMPtr<nsIMessageView> mMessageView;
  nsCOMPtr<nsIMsgFolder> mOpenFolder;
  nsCOMPtr<nsIMsgWindowCommands> mMsgWindowCommands;

  // let's not make this a strong ref - we don't own it.
  nsWeakPtr mRootDocShellWeak;
  nsWeakPtr mMessageWindowDocShellWeak;

  nsString mMailCharacterSet;

  // small helper function used to optimize our use of a weak reference
  // on the message window docshell. Under no circumstances should you be holding on to
  // the docshell returned here outside the scope of your routine.
  void GetMessageWindowDocShell(nsIDocShell ** aDocShell);
};

#endif
