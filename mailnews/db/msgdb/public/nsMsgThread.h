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

#ifndef _nsMsgThread_H
#define _nsMsgThread_H

#include "nsIMsgThread.h"
#include "nsString.h"
#include "MailNewsTypes.h"

class nsIMdbTable;
class nsIMessage;
class nsMsgDatabase;

class nsMsgThread : public nsIMsgThread {
public:
	nsMsgThread();
	nsMsgThread(nsMsgDatabase *db, nsIMdbTable *table);
	virtual ~nsMsgThread();

    NS_DECL_ISUPPORTS

	NS_IMETHOD		SetThreadKey(nsMsgKey threadKey);
	NS_IMETHOD		GetThreadKey(nsMsgKey *result);
    NS_IMETHOD		GetFlags(PRUint32 *result);
    NS_IMETHOD		SetFlags(PRUint32 flags);
	NS_IMETHOD		GetNumChildren(PRUint32 *result);
	NS_IMETHOD		GetNumUnreadChildren (PRUint32 *result);
	NS_IMETHOD		AddChild(nsIMessage *child, PRBool threadInThread);
	NS_IMETHOD		GetChildAt(PRUint32 index, nsIMessage **result);
	NS_IMETHOD		GetChild(nsMsgKey msgKey, nsIMessage **result);
	NS_IMETHOD		GetChildHdrAt(PRUint32 index, nsIMessage **result);
	NS_IMETHOD 		RemoveChildAt(PRUint32 index);
	NS_IMETHOD		RemoveChild(nsMsgKey msgKey);
	NS_IMETHOD		MarkChildRead(PRBool bRead);

	// non-interface methods
    nsIMdbTable		*GetMDBTable() {return m_mdbTable;}
	nsIMdbRow		*GetMetaRow() {return m_metaRow;}
	nsMsgDatabase	*m_mdbDB ;

protected:

	void				Init();
	virtual nsresult	InitCachedValues();
	nsresult			ChangeChildCount(PRInt32 delta);
	nsresult			ChangeUnreadChildCount(PRInt32 delta);

	nsMsgKey		m_threadKey; 
	PRUint32		m_numChildren;		
	PRUint32		m_numUnreadChildren;	
	PRUint32		m_flags;
    nsIMdbTable		*m_mdbTable;
	nsIMdbRow		*m_metaRow;
	PRBool			m_cachedValuesInitialized;



};

#endif

