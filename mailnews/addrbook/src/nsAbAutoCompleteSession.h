/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsAbAutoCompleteSession_h___
#define nsAbAutoCompleteSession_h___

#include "nsIAutoCompleteSession.h"
#include "nsIAutoCompleteListener.h"
#include "nsCOMPtr.h"
#include "msgCore.h"

class nsAbAutoCompleteSession : public nsIAutoCompleteSession
{
public:
	NS_DECL_ISUPPORTS
	nsAbAutoCompleteSession();
	virtual ~nsAbAutoCompleteSession();

	NS_IMETHOD AutoComplete(const PRUnichar *aSearchString, nsIAutoCompleteListener *aResultListener); 

protected:
	nsCOMPtr<nsIAutoCompleteListener> m_resultListener;
};

// factory method
extern nsresult NS_NewAbAutoCompleteSession(const nsIID &aIID, void ** aInstancePtrResult);

#endif /* nsAbAutoCompleteSession_h___ */


