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

#ifndef _nsMessageView_h
#define _nsMessageView_h

#include "nsIMessageView.h"

class nsMessageView : public nsIMessageView {

public:

    NS_DECL_ISUPPORTS

	nsMessageView();
	~nsMessageView();
	nsresult Init();
	NS_DECL_NSIMESSAGEVIEW

protected:
	PRBool mShowThreads;
	PRUint32 mViewType;

};

NS_BEGIN_EXTERN_C

nsresult
NS_NewMessageView(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif
