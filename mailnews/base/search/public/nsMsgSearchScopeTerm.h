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

#ifndef __nsMsgSearchScopeTerm_h
#define __nsMsgSearchScopeTerm_h

#include "nsMsgSearchCore.h"
#include "nsMsgSearchArray.h"
#include "nsIMsgSearchAdapter.h"
#include "nsIMsgFolder.h"
#include "nsIMsgSearchAdapter.h"
#include "nsFileStream.h"
#include "nsCOMPtr.h"

class nsMsgSearchScopeTerm 
{
public:
	nsMsgSearchScopeTerm (nsMsgSearchScopeAttribute, nsIMsgFolder *);
	nsMsgSearchScopeTerm ();
	virtual ~nsMsgSearchScopeTerm ();

	PRBool IsOfflineNews();
	PRBool IsOfflineMail ();
	PRBool IsOfflineIMAPMail();  // added by mscott 
	nsresult GetMailPath(nsIFileSpec **aFileSpec);
	nsresult TimeSlice ();

	nsresult InitializeAdapter (nsMsgSearchTermArray &termList);

	char *GetStatusBarName ();

	nsMsgSearchScopeAttribute m_attribute;
	char *m_name;
	nsCOMPtr <nsIMsgFolder> m_folder;
	nsIOFileStream		*m_fileStream;
	nsCOMPtr <nsIMsgSearchAdapter> m_adapter;
	PRBool m_searchServer;

};

#endif
