/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
	
	NS_IMETHOD		Open(nsIFileSpec *folderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB);

  NS_IMETHOD    StartBatch();
  NS_IMETHOD    EndBatch();
	NS_IMETHOD    SetSummaryValid(PRBool valid = TRUE);
  NS_IMETHOD    DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator);
	
protected:
	// IMAP does not set local file flags, override does nothing
	virtual void	UpdateFolderFlag(nsIMsgDBHdr *msgHdr, PRBool bSet, 
									 MsgFlags flag, nsIOFileStream **ppFileStream);

};


#endif
