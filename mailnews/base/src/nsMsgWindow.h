/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMsgWindow_h
#define _nsMsgWindow_h

#include "nsIMsgWindow.h"
#include "nsIMsgStatusFeedback.h"
#include "nsITransactionManager.h"
#include "nsIMessageView.h"
#include "nsIMsgFolder.h"
#include "nsIWebShell.h"

#include "nsCOMPtr.h"

class nsMsgWindow : public nsIMsgWindow {

public:

    NS_DECL_ISUPPORTS

	nsMsgWindow();
	virtual ~nsMsgWindow();
	nsresult Init();
	NS_DECL_NSIMSGWINDOW

protected:
	nsCOMPtr<nsIMsgStatusFeedback> mStatusFeedback;
	nsCOMPtr<nsITransactionManager> mTransactionManager;
	nsCOMPtr<nsIMessageView> mMessageView;
	nsCOMPtr<nsIMsgFolder> mOpenFolder;
	// let's not make this a strong ref - we don't own it.
	nsIWebShell *mRootWebShell;
};



#endif
