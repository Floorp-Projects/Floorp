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
#ifndef _nsImapMailDatabase_H_
#define _nsImapMailDatabase_H_

#include "nsMailDatabase.h"

class nsImapMailDatabase : public nsMailDatabase
{
public:
	// OK, it's dumb that this should require a fileSpec, since there is no file
	// for the folder. This is mainly because we're deriving from nsMailDatabase;
	// Perhaps we shouldn't...
	nsImapMailDatabase();
	virtual ~nsImapMailDatabase();
	
	nsresult		Open(PRBool create, nsImapMailDatabase** pMessageDB, PRBool *dbWasCreated);
	
	NS_IMETHOD			SetSummaryValid(PRBool valid = TRUE);
	
protected:
	// IMAP does not set local file flags, override does nothing
	virtual void	UpdateFolderFlag(nsMsgHdr *msgHdr, PRBool bSet, 
									 MsgFlags flag, PRFileDesc *fid);
};


#endif
